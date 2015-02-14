/*****************************************************************************
 *  Copyright Statement:
 *  --------------------
 *  MobileTech(Shanghai) Co., Ltd.
 *
 *****************************************************************************/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   oa_soc.c
 *
 * Project:
 * --------
 *   OPEN AT project
 *
 * Description:
 * ------------
 *   This module contains gprs socket connect for OPENAT.
 *
 * Author:
 * -------
 *   simon.
 *
 ****************************************************************************/
#include "oa_type.h"
#include "oa_api.h"
#include "oa_setting.h"
#include "oa_soc.h"
#include "oa_ftp.h"
#include "oa_crc.h"
#include "oa_location.h"
#include "oa_adc.h"
#include "oa_common.h"
#include "oa_alarm.h"
#include "oa_at.h"
#include "oa_eint.h"
#include "oa_sms.h"
#include "oa_gps.h"

/*debug*/
#define GPRS_DEBUG OA_DEBUG_USER

/*
* do not re-connect the network immediately after the old one closed.
* wait a period of time (for example, 5sec) to detach network,and then re-connect
*/
#define OA_GPRS_WAITING_RELEASING 10000 /*mSec*/

//Gprs heartbeat time, for keep gprs connecting, default 60s, if not use heartbeat, set OA_GPRS_HEARTBEAT_TIME = 0
#define OA_GPRS_HEARTBEAT_TIME 60

//Gprs rx/tx buffer length
#define OA_MAX_SOC_RCV_BUFLEN  4096
#define OA_MAX_SOC_SEND_BUFLEN  20480

oa_uint8 gprs_rx_buffer[OA_MAX_SOC_RCV_BUFLEN+1]={0}; // gprs接收数据缓冲区, 30K bytes
oa_uint8 gprs_tx_buffer[OA_MAX_SOC_SEND_BUFLEN+1]={0}; // gprs 发送缓冲区，gprs一次最大能发送1400 bytes

oa_uint8 g_return_for_server[100] = {0};
oa_comm_protocol_enum g_current_comm_protocol = OA_COMM_NULL;

oa_soc_context g_soc_context = {0};
extern void oa_soc_set_apn_cb(oa_bool result);
extern void oa_soc_fill_addr_struct(void);

//for NOINIT buffer
#define OA_GPRS_BUFFER_NOINIT_SIZE (oa_sram_noinit_get_size() - (1024*50))//500k-50k?

void oa_soc_init_fast(void)
{
    //init socket context
    g_soc_context.state = OA_SOC_STATE_OFFLINE;
    g_soc_context.socket_id = -1;
    g_soc_context.can_connect = OA_FALSE;
    g_soc_context.is_blocksend = OA_FALSE;
    g_soc_context.recon_counter = 0;

    oa_strncpy(g_soc_context.apn_info.apn, "cmnet", oa_strlen("cmnet"));
    oa_strncpy(g_soc_context.apn_info.userName, "root", oa_strlen("root"));
    oa_strncpy(g_soc_context.apn_info.password, "root", oa_strlen("root"));
    
#if 0	
    /*create gprs rx/tx ring buffers*/
    //g_soc_context.gprs_tx = oa_create_ring_buffer(gprs_tx_buffer, sizeof(gprs_tx_buffer));
    g_soc_context.gprs_rx = oa_create_ring_buffer(gprs_rx_buffer, sizeof(gprs_rx_buffer));
#endif
	/*using noinitial buffer*/
	//g_soc_context.gprs_tx = oa_create_ring_buffer_noinit(NULL, OA_GPRS_BUFFER_NOINIT_SIZE);

    //fill socket connect server address 
    oa_soc_fill_addr_struct();
}

void oa_app_soc_can_connect(void *param)
{
    OA_DEBUG_USER("%s called.", __func__);
    g_soc_context.can_connect = OA_TRUE;
}

void oa_soc_notify_ind_user_callback(void *inMsg);
void oa_soc_init(void)
{
    //register network APN, only set APN success,can connect network
    //oa_gprs_set_apn_req(&g_soc_context.apn_info, oa_soc_set_apn_cb);
    oa_gprs_apn_init(&g_soc_context.apn_info);

    //register socket network notify event callback function 
    oa_soc_notify_ind_register(oa_soc_notify_ind_user_callback);

    oa_evshed_start(OA_EVSHED_ID_0, oa_app_soc_can_connect, NULL, 3000);
}

void oa_soc_fill_addr_struct(void)
{
    g_soc_context.addr_type = OA_IP_ADDRESS;
    g_soc_context.soc_addr.addr_len = 4;
    g_soc_context.soc_addr.port = g_soc_param.port;
    g_soc_context.soc_addr.sock_type = g_soc_param.connct_type;
    
    if(!oa_strcmp(g_soc_param.serve_ipaddr, (oa_char*)"0.0.0.0"))
    {
        g_soc_context.addr_type = OA_DOMAIN_NAME;
        oa_memcpy(g_soc_context.soc_addr.addr, g_soc_param.serve_host_name, oa_strlen(g_soc_param.serve_host_name));
    }
    else
    {
        g_soc_context.addr_type = OA_IP_ADDRESS;
        oa_memcpy(g_soc_context.soc_addr.addr, g_soc_param.serve_ipaddr, oa_strlen(g_soc_param.serve_ipaddr));
    }
    OA_DEBUG_USER("oa_soc_fill_addr_struct ip:%s,port:%d", g_soc_param.serve_ipaddr, g_soc_param.port);
}

void oa_soc_set_apn_cb(oa_bool result)
{
    OA_DEBUG_USER("%s:set apn ok! %s, %s, %s", __func__, g_soc_context.apn_info.apn, g_soc_context.apn_info.userName, g_soc_context.apn_info.password);
    g_soc_context.can_connect = OA_TRUE;
}

oa_uint8 oa_soc_connect_req(void)
{
    oa_int16 ret = OA_SOC_SUCCESS;

    oa_soc_fill_addr_struct();

    if(g_soc_context.soc_addr.sock_type == OA_SOCK_STREAM 
     ||g_soc_context.soc_addr.sock_type == OA_SOCK_DGRAM )
    {
        g_soc_context.socket_id = oa_soc_create(g_soc_context.soc_addr.sock_type, 0);
        if(g_soc_context.socket_id >= 0)
        {
            OA_DEBUG_USER("%s:sock_id=%d create ok!", __func__, g_soc_context.socket_id);
        }
        else
        {
            OA_DEBUG_USER("%s:oa_soc_create failed!", __func__);
            return OA_SOC_ERROR;
        }
    }
    else
    {
        OA_DEBUG_USER("%s:sock_type invalid!", __func__);
        return OA_SOC_ERROR;
    }

    OA_DEBUG_USER("Ready to connect:addr=%s,port=%d,soc_type=%d,addr_type=%d", 
                   g_soc_context.soc_addr.addr, 
                   g_soc_context.soc_addr.port, 
                   g_soc_context.soc_addr.sock_type,
                   g_soc_context.addr_type);

    ret = oa_soc_connect(g_soc_context.socket_id, g_soc_context.addr_type, &g_soc_context.soc_addr);

    if(ret == OA_SOC_WOULDBLOCK)
    {  
        OA_DEBUG_USER("%s:sock_id=%d connect block waiting!",__func__,g_soc_context.socket_id);
        g_soc_context.state = OA_SOC_STATE_CONNECT;
    }
    else if(ret == OA_SOC_SUCCESS)
    {
        OA_DEBUG_USER("%s:sock_id=%d connect ok!",__func__,g_soc_context.socket_id);
        g_soc_context.state = OA_SOC_STATE_ONLINE;
    }
    else
    {
        OA_DEBUG_USER("%s:sock_id=%d connect fail ret=%d!",__func__,g_soc_context.socket_id,ret);
        oa_soc_close_req();
        return OA_SOC_ERROR;
    }

    return OA_SOC_SUCCESS;
}

void oa_soc_can_reconnect_again(void* param)
{
    OA_DEBUG_USER("%s called", __func__);
    g_soc_context.can_connect = OA_TRUE;
}

void oa_soc_close_req(void)
{
    oa_int8 ret = 0;
    
    g_soc_context.can_connect = OA_FALSE;
    g_soc_context.is_blocksend = OA_FALSE;

    if(g_soc_context.state == OA_SOC_STATE_OFFLINE) 
        return;

    if(g_soc_context.socket_id >= 0)
        ret = oa_soc_close(g_soc_context.socket_id);

    OA_DEBUG_USER("%s:socket_id=%d,ret=%d", __func__, g_soc_context.socket_id, ret);

    g_soc_context.state = OA_SOC_STATE_OFFLINE;
    g_soc_context.socket_id = -1;
    
    g_LoginStatus = LOGIN_CONNECT;
}

void oa_return_for_server(oa_uint8 len)
{
    oa_uint8 n = 0;
    oa_uint16 crcvalue = 0;

    g_return_for_server[0] = 0xFF;
    g_return_for_server[1] = 0xFF;
    g_return_for_server[2] = 0;   //length
    g_return_for_server[3] = g_serial_number/256;
    g_return_for_server[4] = g_serial_number%256;
    g_serial_number++;

    if (0x8001 == g_serial_number)
    {
        g_serial_number = 0;
    }
    
    //g_return_for_server[5] = 0x05;
    
    if (g_current_comm_protocol == OA_COMM_NULL)
    {
        OA_DEBUG_USER("g_current_comm_protocol == OA_COMM_NULL");
        return;
    }
    g_return_for_server[5] = g_current_comm_protocol;
    g_return_for_server[2] = len + 4; //length

    crcvalue = u16CRC_Calc16(g_return_for_server, len);
    g_return_for_server[len++] = crcvalue/256;
    g_return_for_server[len++] = crcvalue%256;
    g_return_for_server[len++] = 0xEE;
    g_return_for_server[len++] = 0xEE;
    //g_return_for_server[2] = len;   //length
    oa_soc_send(g_soc_context.socket_id, g_return_for_server, len);
}

void oa_gprs_data_handler(oa_uint8 *pData, oa_uint16 pLen)
{
    oa_uint8 zero = '0';
    oa_uint8 point = '.';
    oa_uint8 i = 0;
    oa_uint8 len = 0;
    oa_uint8 protocol = 0;
    oa_uint8 temp[4] = 0;
    oa_uint8 IMEI[9] = {0};
    oa_uint8 *data = NULL;
    oa_uint8 *pFtp = "ftp:";
    oa_char buf[32];
    oa_char sendStr[200]={0};
    oa_char sendUcs2Str[100]={0};
    oa_time_struct time = {0};

    len = pData[0];
    //pData[1];     //server serial number
    //pData[2];
    oa_memset(g_return_for_server, 0x00, sizeof(g_return_for_server));
    i = 7;

    switch (pData[3])
    {
        case OA_COMM_WORKMODE://work mode
            OA_DEBUG_USER("work mode");
            g_oa_wristband_parameter.mode = pData[4];
            
            if (OA_TRUE == oa_soc_setting_save(SETTING_WRISTBAND))
            {
                g_return_for_server[i++] = pData[4];
            }
            else
            {
                g_return_for_server[i++] = 0x60;
            }
            g_current_comm_protocol = OA_COMM_WORKMODE;
            oa_return_for_server(i);
            break;

        case OA_COMM_SENDFREQ://send freq
            OA_DEBUG_USER("send freq");
            g_oa_wristband_parameter.period = pData[4]*256 + pData[5];

            if (OA_TRUE == oa_soc_setting_save(SETTING_WRISTBAND))
            {
                g_return_for_server[i++] = pData[4];
                g_return_for_server[i++] = pData[5];
            }
            else
            {
                g_return_for_server[i++] = 0x60;
            }
            g_current_comm_protocol = OA_COMM_SENDFREQ;
            oa_return_for_server(i);
            break;
            
        case OA_COMM_HEARTBEATFREQ:
            OA_DEBUG_USER("heartbeat freq");
            g_oa_wristband_parameter.Heartbeatperiod = pData[4]*256 + pData[5];

            if (OA_TRUE == oa_soc_setting_save(SETTING_WRISTBAND))
            {
                g_return_for_server[i++] = pData[4];
                g_return_for_server[i++] = pData[5];
            }
            else
            {
                g_return_for_server[i++] = 0x60;
            }
            g_current_comm_protocol = OA_COMM_HEARTBEATFREQ;
            oa_return_for_server(i);
            break;

        case OA_COMM_ALARMSW://alarm status
            OA_DEBUG_USER("alarm status");
            g_oa_wristband_parameter.alarm = pData[4];

            if (OA_TRUE == oa_soc_setting_save(SETTING_WRISTBAND))
            {
                g_return_for_server[i++] = pData[4];
            }
            else
            {
                g_return_for_server[i++] = 0x60;
            }
            g_current_comm_protocol = OA_COMM_ALARMSW;
            oa_return_for_server(i);
            break;

        case OA_COMM_IP://ip&port
            OA_DEBUG_USER("ip&port");
            oa_soc_close_req();
            oa_memset(g_soc_param.serve_ipaddr, 0, SET_IPADDR_LEN + 1);

            for (i = 4; i < 8; i++)
            {
                oa_itoa(pData[i], temp, 10);
                oa_strcat(g_soc_param.serve_ipaddr, temp);

                if (i != 7)
                {
                    oa_strcat(g_soc_param.serve_ipaddr, &point);
                }
            }
            g_soc_param.port = pData[8]*256 + pData[9];
            OA_DEBUG_USER("ip=%s\r\nport=%d", g_soc_param.serve_ipaddr, g_soc_param.port);

            if (OA_TRUE == oa_soc_setting_save(SETTING_SOC))
            {
                for (i = 7; i < 15; i++)
                {
                    g_return_for_server[i] = pData[i- 3];
                }
                //i++; //same to below OA_COMM_BANDPHONE
            }
            else
            {
                g_return_for_server[i++] = 0x60;
            }
            g_current_comm_protocol = OA_COMM_IP;
            oa_return_for_server(i);
            //oa_module_restart(NULL);
            break;

        case OA_COMM_BANDPHONE://bound phone number
            OA_DEBUG_USER("bound phone number");

            if (pData[10] < 4)//only two bound number 1&2, add 3, can not be modefied
            {
                oa_memset(g_oa_wristband_parameter.phonenumber[pData[10] - 1], 0, PHONE_NUMBER_LEN);

                for (i = 4; i < 10; i++)
                {
                    oa_itoa(pData[i], temp, 16);

                    /*if phone number data is 0x02, to string is 2, so should add 0 before 2*/
                    if ((i != 4)&&(1 == oa_strlen(temp)))
                    {
                        oa_strcat(g_oa_wristband_parameter.phonenumber[pData[10] - 1], &zero);
                    }
                    oa_strcat(g_oa_wristband_parameter.phonenumber[pData[10] - 1], temp);
                }
                OA_DEBUG_USER("bound phone number[%d] = %s", pData[10], g_oa_wristband_parameter.phonenumber[pData[10] - 1]);

                if (OA_TRUE == oa_soc_setting_save(SETTING_WRISTBAND))
                {
                    //add one byte for phonenumber NO.
                    for (i = 7; i < 14; i++)
                    {
                        g_return_for_server[i] = pData[i - 3];
                    }
                    //i++; //!!!notice:big BUG, return i = 14!!!
                }
                else
                {
                    g_return_for_server[i++] = 0x60;
                }
            }
            else
            {
                g_return_for_server[i++] = 0x60;
            }
            g_current_comm_protocol = OA_COMM_BANDPHONE;
            oa_return_for_server(i);
            break;

        case OA_COMM_RESTART://restart
            OA_DEBUG_USER("restart");
            oa_module_restart(NULL);
            //GPS_Power(ON);    //only for test
            break;

        case OA_COMM_CHECKSTATUS://check status
            OA_DEBUG_USER("check status");
            g_return_for_server[i++] = g_oa_wristband_parameter.mode;
            g_return_for_server[i++] = g_oa_wristband_parameter.period/256;
            g_return_for_server[i++] = g_oa_wristband_parameter.period%256;
            g_return_for_server[i++] = g_BatteryPercent;
            g_return_for_server[i++] = g_ChargeStatus;
            g_return_for_server[i++] = g_eint_valid;
            g_current_comm_protocol = OA_COMM_CHECKSTATUS;
            oa_return_for_server(i);
            break;

        case OA_COMM_CALLTHEROLL://call the roll
            OA_DEBUG_USER("call the roll");
            #if 0
            g_return_for_server[i++] = g_oa_wristband_parameter.mode;
            g_return_for_server[i++] = g_oa_wristband_parameter.period/256;
            g_return_for_server[i++] = g_oa_wristband_parameter.period%256;
            g_return_for_server[i++] = g_BatteryPercent;
            g_return_for_server[i++] = g_ChargeStatus;
            g_current_comm_protocol = OA_COMM_CALLTHEROLL;
            oa_return_for_server(i);
            #endif
            g_oa_location_mode = oa_location_passivity;
            break;

        case OA_COMM_UPGRADE://upgrade
            if ((g_BatteryPercent >= 10)&&(g_upgrade == OA_FALSE))
            {
                OA_DEBUG_USER("upgrade");
                oa_enable_low_power_sleep(OA_FALSE);
                GPS_Power(DOWN);
                g_gps_poweron = OA_FALSE;
                g_Str_GPS_GPGGA.validity = 0;
                g_upgrade = OA_TRUE;
                //oa_soc_close_req();
                oa_fdelete(OA_SOC_SETTING_FILE);
                oa_fdelete(OA_WRISTBAND_SETTING_FILE);
                oa_strcat(pFtp, g_oa_wristband_parameter.ftp);
                oa_strcat(pFtp, ",21,admin,admin,openatdll.dll,0");
                ftp_core_step_handler(pFtp);
            }
            else
            {
                OA_DEBUG_USER("low battery, cannot upgrade!!!");
            }
            break;

        case OA_COMM_CALLBACK:
            OA_DEBUG_USER("callback");
            oa_sprintf(buf, "ATD%s;", g_oa_wristband_parameter.phonenumber[pData[4] - 1]);
            oa_at_cmd_demo(buf);
            oa_at_cmd_demo_submit();
            break;

        case OA_COMM_CHECKTIME://time
            OA_DEBUG_USER("time");
            time.nYear = pData[4] + 2000;
            time.nMonth = pData[5];
            time.nDay = pData[6];
            time.nHour = pData[7];
            time.nMin = pData[8];
            time.nSec = pData[9];
            oa_set_time(&time);
            OA_DEBUG_USER("time:%d-%d-%d----%d:%d:%d",time.nYear,time.nMonth,time.nDay,time.nHour,time.nMin,time.nSec);
            g_return_for_server[i++] = 0x00;
            oa_return_for_server(i);
            break;

        case OA_COMM_GENERALRETURN://general return
            OA_DEBUG_USER("gegeral return");
            //pData[4];     //wristband serial number
            //pData[5];
            g_Alarm_Mode = Alarm_NULL;
            g_eint_valid = 0;
            g_send_count = 58;
            g_DisassemblyStatus = 0;
            break;

        case OA_COMM_LOGINRETURN://login return
            OA_DEBUG_USER("login return");
            //pData[4];     //wristband serial number
            //pData[5];
            
            if (0 == pData[6])
            {
                g_LoginStatus = 2;
                OA_DEBUG_USER("login OK");
            }
            else
            {
                OA_DEBUG_USER("login error");
            }
            break;

        case OA_COMM_LOCATIONRETURN:
            OA_DEBUG_USER("location return");
            g_current_band_number = pData[4] - 1;
            oa_memcpy(sendStr, &(pData[5]), len - 11);
            oa_chset_convert(OA_CHSET_GB2312,
                             OA_CHSET_UCS2,
                             sendStr,
                             sendUcs2Str,
                             100);
            OA_DEBUG_USER("%s", sendStr);
            oa_sms_send((oa_char *)g_oa_wristband_parameter.phonenumber[g_current_band_number], sendUcs2Str, oa_wstrlen((oa_wchar *)sendUcs2Str), OA_SMSAL_UCS2_DCS);
            break;

        case OA_COMM_VERSION:
            OA_DEBUG_USER("version");
            g_return_for_server[i++] = g_versionH;
            g_return_for_server[i++] = g_versionL;
            g_current_comm_protocol = OA_COMM_VERSION;
            oa_return_for_server(i);
            break;

        default:
            break;
    }
}

void oa_soc_gprs_recv(oa_uint8 *data, oa_uint16 len)
{
    oa_uint8 i = 0;
    oa_uint8 Tempdata[100] = {0x00};
    oa_uint16 Templen = 0x00;
    oa_uint16 crcvalue = 0x00;
    
    for (i = 0; i < len; i++)
    {
        OA_DEBUG_USER("data[%d]=%02X", i, data[i]);
    }

    if (LOGIN_SUCCESS == g_LoginStatus)
    {
        while (len)
        {
            if (data[2] == len)
            {
                Templen = data[2];
                oa_memset(Tempdata, 0x00, Templen);
                oa_memcpy(Tempdata, data, Templen);
            }
            else
            {
                OA_DEBUG_USER("data length error");
                return;
            }

            if (len <= Templen)
            {
                len = 0;
            }
            else
            {
                len -= Templen;
            }
            
            data += Templen;
            
            if ((0xEE == Tempdata[0])
              &&(0xEE == Tempdata[1])
              &&(0xFF == Tempdata[Templen-2])
              &&(0xFF == Tempdata[Templen-1]))
            {
                //CRC check...
                crcvalue = u16CRC_Calc16(Tempdata, Templen - 4);

                if (crcvalue == (Tempdata[Templen-4]*256 + Tempdata[Templen-3]))
                {
                    //Templen -= 4;
                    //Tempdata += 2;
                    OA_DEBUG_USER("oa_soc_gprs_recv check OK");
                    oa_gprs_data_handler(Tempdata+2, Templen-4);
                }
                else
                {
                    OA_DEBUG_USER("oa_soc_gprs_recv CRC check error");
                }
            }
            else
            {
                //OA_DEBUG_USER("data=%s", data);
            }
        }
    }
    else//check login
    {
        if (data[2] == len)
        {
            Templen = data[2];
        }
        else
        {
            OA_DEBUG_USER("data length error");
            return;
        }
        
        if ((0xEE == data[0])&&(0xEE == data[1])&&(0xFF == data[Templen-2])&&(0xFF == data[Templen-1]))
        {
            crcvalue = u16CRC_Calc16(data, len - 4);
            if (crcvalue == (data[len-4]*256 + data[len-3]))
            {
                if (data[5] == OA_COMM_LOGINRETURN)
                {
                    if (0 == data[8])
                    {
                        g_LoginStatus = LOGIN_SUCCESS;
                        OA_DEBUG_USER("-login OK-");
                    }
                    else
                    {
                        OA_DEBUG_USER("-login error-");
                    }
                }
            }
            else
            {
                OA_DEBUG_USER("-oa_soc_gprs_recv CRC check error-");
            }
        }
    }
}

//Socket Notify Event indication handler framework
void oa_soc_notify_ind_user_callback(void *inMsg)
{
    oa_char *pHead = NULL;
    oa_char *pTall = NULL;
    oa_char buf[8] = {0};
    oa_int32 ret = 0;    
    oa_app_soc_notify_ind_struct *soc_notify = (oa_app_soc_notify_ind_struct*) inMsg;

    ftp_app_socket_notify_ind((void *)soc_notify);

    if(soc_notify->socket_id == g_agps_socketID)
    {
        OA_DEBUG_USER("g_agps_socketID=%d, event_type=%02X", soc_notify->socket_id, soc_notify->event_type);         
        if (soc_notify->event_type == 1)//read data
        {
            do
            {
                memset(gprs_rx_buffer, 0 , (OA_MAX_SOC_RCV_BUFLEN*sizeof(oa_uint8))); 
                ret = oa_soc_recv(soc_notify->socket_id , (oa_uint8*)gprs_rx_buffer, OA_MAX_SOC_RCV_BUFLEN, 0);
                if(ret > 0)
                {   
                    OA_DEBUG_USER("sock_id=%d read ok ret=%d!",soc_notify->socket_id,ret);
                    OA_DEBUG_USER("%s", gprs_rx_buffer);

                    pHead = oa_strchr((oa_char *)gprs_rx_buffer, ':');
                    if (pHead == NULL)
                    {
                        OA_DEBUG_USER("error");
                        return;
                    }
                    pHead++;
                    pTall = oa_strstr((oa_char *)pHead, "\r\n");
                    if (pTall == NULL)
                    {
                        OA_DEBUG_USER("error");
                        return;
                    }
                    oa_memcpy(buf, pHead, pTall - pHead);
                    ret = oa_atoi(buf);
                    if (ret < 100)
                    {
                        OA_DEBUG_USER("error");
                        return;
                    }
                    OA_DEBUG_USER("len=%d", ret);
                    
                    pTall = oa_strstr((oa_char *)pHead, "\r\n\r\n");
                    oa_uart_write(OA_UART2, pTall + 4, ret);
                    oa_soc_close(g_agps_socketID);
                    g_agps_socketID = -1;
               }
               else
               {
                   //OA_DEBUG_USER("read data error or block");
                   return;
               }  
           }while(ret>0);
        }
        else if (soc_notify->event_type == 8)
        {
            oa_soc_send(soc_notify->socket_id, "cmd=aid;user=lirui@cniia.com;pwd=Bvjdj;lat=32.07262;lon=118.745304;pacc=100000\n",
                                     oa_strlen("cmd=aid;user=lirui@cniia.com;pwd=Bvjdj;lat=32.07262;lon=118.745304;pacc=100000\n"));
        }

       return;
    }

    //if other application's socket id, ignore it.
    if(soc_notify->socket_id != g_soc_context.socket_id)
    {
       OA_DEBUG_USER("%s:sock_id=%d unknow, event_type=%X!",__func__,soc_notify->socket_id,soc_notify->event_type);    
       return;
    }
    
    switch (soc_notify->event_type)
    {
        case OA_SOC_WRITE:
        {
           if (soc_notify->result == OA_TRUE)
           {
               g_soc_context.is_blocksend = OA_FALSE;
               OA_DEBUG_USER("%s:sock_id=%d resend!",__func__,soc_notify->socket_id);
           }
           else
           {
               OA_DEBUG_USER("%s:sock_id=%d send fail err=%d!",__func__,soc_notify->socket_id,soc_notify->error_cause);
               oa_soc_close_req( );
           }     
           
           break;
        }
            
        case OA_SOC_READ:
        {
            if (soc_notify->result == OA_TRUE)
            {
                do
                {
                    memset(gprs_rx_buffer, 0 , (OA_MAX_SOC_RCV_BUFLEN*sizeof(oa_uint8))); 
                    ret = oa_soc_recv(soc_notify->socket_id , (oa_uint8*)gprs_rx_buffer, OA_MAX_SOC_RCV_BUFLEN, 0);
  
                    if(ret > 0)
                    {   
                        OA_DEBUG_USER("%s:sock_id=%d read ok ret=%d!",__func__,soc_notify->socket_id,ret);
                        oa_soc_gprs_recv((oa_uint8*)gprs_rx_buffer, ret);
                    }
                    else
                    {
                        //read data error or block
                    }  
                }while(ret>0); //Make sure use  do{...}while  to read out all received data.
            }
            else
            {
                OA_DEBUG_USER("%s:sock_id=%d read fail err=%d!",__func__,soc_notify->socket_id,soc_notify->error_cause);
                oa_soc_close_req( );
            }     
            break;
        }
            
        case OA_SOC_CONNECT:
        {
            if (soc_notify->result == OA_TRUE)
            {
                OA_DEBUG_USER("%s:sock_id=%d connect ok!",__func__,soc_notify->socket_id);
                g_soc_context.state = OA_SOC_STATE_ONLINE;
            }
            else
            {
                OA_DEBUG_USER("%s:sock_id=%d connect fail err=%d!",__func__,soc_notify->socket_id,soc_notify->error_cause);
                oa_soc_close_req( );
            }
            break;
        }       
            
        case OA_SOC_CLOSE:
        {
            OA_DEBUG_USER("%s:sock_id=%d close,error_cause=%d",__func__,soc_notify->socket_id,soc_notify->error_cause);
            oa_soc_close_req();
            break;
        }
            
        case OA_SOC_ACCEPT:
            break;
            
        default:
            break;
    }
}

