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
 *   oa_app.c
 *
 * Project:
 * --------
 *   OPEN AT project
 *
 * Description:
 * ------------
 *   This Module gives a sample app on openat framework.
 *
 * Author:
 * -------
 *   None!
 *
 ****************************************************************************/
#include "oa_type.h"
#include "oa_api.h"
#include "oa_soc.h"
#include "oa_at.h"
#include "oa_sms.h"
#include "oa_setting.h"
#include "oa_ftp.h"
#include "oa_adc.h"
#include "oa_eint.h"
#include "oa_crc.h"
#include "oa_location.h"
#include "oa_alarm.h"
#include "oa_common.h"
#include "oa_heartbeat.h"
#include "oa_gps.h"

#if 1
#define APP_Debug  OA_DEBUG_USER
#else
#define APP_Debug
#endif

oa_char OA_VERSION_NO[] = "v6.1.5ep";
//g_versionH.g_versionL
oa_uint8 g_versionH = 0x02;
oa_uint8 g_versionL = 0x17;

oa_bool   g_upgrade             = OA_FALSE;
oa_bool   g_gps_poweron         = OA_FALSE;
oa_bool   g_lowpopwer_sw        = OA_FALSE;
oa_int8   g_agps_socketID       = -1;
oa_uint8  g_error_count         = 0;
oa_uint8  g_locate_count        = 0x00;//if located by LBS, g_locate_count++, if g_locate_count>=10, GPS will be turn on until locating
oa_uint8  g_GSMSignalLevel      = 0x00;
oa_uint8  g_ChargeStatus        = 0x00;
oa_uint8  g_DisassemblyStatus   = 0x00;
OA_LOGIN_ENUM  g_LoginStatus    = LOGIN_CONNECT;
oa_uint8  g_login[23]           = {0x00};
oa_uint16 g_serial_number       = 0;
//oa_uint32 g_LBS_Count           = 0x00;
//oa_uint32 g_gps_count           = 0;
oa_uint32 g_main_period         = 1;
oa_uint32 main_loop             = 0;

oa_uart_struct g_uart2_port_setting = 
{
    9600,                       /* baudrate */
    oa_uart_len_8,              /* dataBits; */
    oa_uart_sb_1,               /* stopBits; */
    oa_uart_pa_none,            /* parity; */
    oa_uart_fc_none,            /* flow control */
    0x11,                       /* xonChar; */
    0x13,                       /* xoffChar; */
    OA_FALSE
};

oa_trace_level_enum g_oa_trace_level = OA_TRACE_USER;       //Define debug information output level.
oa_trace_port_enum  g_oa_trace_port  = OA_TRACE_PORT_1;     //Define debug information output port.
oa_uart_enum        g_oa_ps_port     = OA_UART_USB2;          //Define the AT ps port, If set OA_UART_AT(virtual AT port) , can release UART1
oa_uart_enum        g_oa_tst_port    = OA_UART_NULL;         //Define the Catcher port,

OA_LED_ENUM g_current_led_status = LED_NULL;

/*****************************************************************
*---Power on hardware init for customer, before openat app runing
*****************************************************************/
oa_bool oa_app_execute_custom_at_cmd(oa_char* pStr, oa_uint16 len)
{
    //APP_Debug("%s: len=%d@@@%s", __func__, len, pStr);
    
    return OA_FALSE;
}

oa_bool oa_IP_isValid(oa_char *ipaddr)
{
    oa_char *temp = NULL;
    oa_char buf[5] = {0};
    oa_uint8 count = 0;

    while (ipaddr != NULL)
    {
        temp = oa_strchr(ipaddr, '.');
        if (temp == NULL)
        {
            if (count == 3)
            {
                if ((oa_atoi(ipaddr) > 255)||(oa_strlen(ipaddr) == 0))
                {
                    return OA_FALSE;
                }
                else
                {
                    return OA_TRUE;
                }
            }
            else
            {
                return OA_FALSE;
            }
        }

        if (0 == (temp - ipaddr))
        {
            return OA_FALSE;
        }
        count++;

        oa_memset(buf, 0, sizeof(buf));
        oa_memcpy(buf, ipaddr, temp - ipaddr);
        if (oa_atoi(buf) > 255)
        {
            return OA_FALSE;
        }

        ipaddr = temp + 1;
    }    
    
    return OA_FALSE;
}

void OA_GPIO_INIT(void)
{
    //output
    //led1 25 red
    oa_gpio_mode_setup(GPIO_PIN_RED, GPIO_MODE);       /*set gpio 3 as 0 mode(gpio modem)*/
    oa_gpio_init(GPIO_OUTPUT, GPIO_PIN_RED);             /*initial gpio 3 as output*/
    oa_gpio_write(GPIO_LOW, GPIO_PIN_RED);            /*write data to gpio 3*/

    //led2 24 blue charge over
    oa_gpio_mode_setup(GPIO_PIN_BLUE, GPIO_MODE);
    oa_gpio_init(GPIO_OUTPUT, GPIO_PIN_BLUE);
    oa_gpio_write(GPIO_LOW, GPIO_PIN_BLUE);

    //WDT 19
    oa_gpio_mode_setup(GPIO_PIN_WDT, GPIO_MODE);
    oa_gpio_init(GPIO_OUTPUT, GPIO_PIN_WDT);
    oa_gpio_write(GPIO_LOW, GPIO_PIN_WDT);
    
    //gps_on/off 61
    oa_gpio_mode_setup(GPIO_PIN_GPS_ONOFF, GPIO_MODE);
    oa_gpio_init(GPIO_OUTPUT, GPIO_PIN_GPS_ONOFF);
    oa_gpio_write(GPIO_LOW, GPIO_PIN_GPS_ONOFF);

    //C_WD 28
    oa_gpio_mode_setup(GPIO_PIN_CWD, GPIO_MODE);
    oa_gpio_init(GPIO_OUTPUT, GPIO_PIN_CWD);
    oa_gpio_write(GPIO_LOW, GPIO_PIN_CWD);

    //charging enbale 55
    oa_gpio_mode_setup(GPIO_PIN_CHARGE_ENABLE, GPIO_MODE);
    oa_gpio_init(GPIO_OUTPUT, GPIO_PIN_CHARGE_ENABLE);
    oa_gpio_write(GPIO_LOW, GPIO_PIN_CHARGE_ENABLE);

    //charging det 62 31
    oa_gpio_mode_setup(GPIO_PIN_CHARGING, GPIO_MODE);
    oa_gpio_init(GPIO_INPUT, GPIO_PIN_CHARGING);

    //charging full 63 30
    oa_gpio_mode_setup(GPIO_PIN_CHARGING_FULL, GPIO_MODE);
    oa_gpio_init(GPIO_INPUT, GPIO_PIN_CHARGING_FULL);
}

void oa_imei_init(void)
{
    oa_uint8 i = 0;
    oa_uint8 n = 0;
    oa_char *pIMEI;

    pIMEI = oa_get_imei();

    //get IMEI error, do what?
    if (49 == pIMEI[0])
    {
        APP_Debug("no cal data");
    }
    
    APP_Debug("OpenAT begin it ok: imei=%s", pIMEI);
    oa_memset(g_IMEI, 0x00, 9);
    
    for (i = 0; i < 8; i++)
    {
        if (0 == i)
        {
            g_IMEI[i] = pIMEI[n] - 48;
            n++;
        }
        else
        {
            g_IMEI[i] = (pIMEI[n] - 48)*16 + (pIMEI[n + 1] - 48);
            n += 2;
        }
        APP_Debug("g_IMEI=%02X", g_IMEI[i]);
    }
}

void oa_login(void)
{
    oa_uint8 i = 0;
    oa_uint16 crcvalue = 0x00;

    oa_memset(g_login, 0x00, sizeof(g_login));

    g_login[0] = 0xFF;
    g_login[1] = 0xFF;
    g_login[2] = 0x16;
    g_login[3] = g_serial_number/256;
    g_login[4] = g_serial_number%256;
    g_serial_number++;
    g_login[5] = 0x00;
    g_login[6] = 0x99;
    g_login[7] = 0x99;
    g_login[8] = 0x99;
    g_login[9] = 0x99;
    g_login[10] = g_IMEI[0];
    g_login[11] = g_IMEI[1];
    g_login[12] = g_IMEI[2];
    g_login[13] = g_IMEI[3];
    g_login[14] = g_IMEI[4];
    g_login[15] = g_IMEI[5];
    g_login[16] = g_IMEI[6];
    g_login[17] = g_IMEI[7];
    crcvalue = u16CRC_Calc16(g_login, 18);
    g_login[18] = crcvalue/256;
    g_login[19] = crcvalue%256;
    g_login[20] = 0xEE;
    g_login[21] = 0xEE;
    
    oa_soc_send(g_soc_context.socket_id, g_login, 22);
}

void oa_led_switch(OA_LED_SW_ENUM led)
{
    if (led == LED_RED)
    {
        oa_gpio_write(GPIO_HIGH, GPIO_PIN_RED);
    }
    else if (led == LED_BLUE)
    {
        oa_gpio_write(GPIO_HIGH, GPIO_PIN_BLUE);
    }
    else if (led == LED_SW_NULL)
    {
        oa_gpio_write(GPIO_LOW, GPIO_PIN_RED);
        oa_gpio_write(GPIO_LOW, GPIO_PIN_BLUE);
    }
}

void oa_led_check(void)
{
    static oa_uint8 flag = 0;
    
    switch (g_current_led_status)
    {
        case LED_RED_ON:
            oa_led_switch(LED_SW_NULL);
            oa_led_switch(LED_RED);
            break;
            
        case LED_BLUE_ON:
            oa_led_switch(LED_SW_NULL);
            oa_led_switch(LED_BLUE);
            break;
            
        case LED_RED_FLASH:
            if (0 == flag)
            {
                oa_led_switch(LED_RED);
                flag = 1;
            }
            else
            {
                oa_led_switch(LED_SW_NULL);
                flag = 0;
            }
            break;
            
        case LED_BLUE_FLASH:
            if (0 == flag)
            {
                oa_led_switch(LED_BLUE);
                flag = 1;
            }
            else
            {
                oa_led_switch(LED_SW_NULL);
                flag = 0;
            }
            break;

        case LED_NULL:
            oa_led_switch(LED_SW_NULL);
            break;

        default:
            break;
    }
}

void oa_current_led(OA_LED_ENUM led)
{
    if (g_current_led_status < led)
    {
        g_current_led_status = led;
    }
    else if (LED_NULL == led)
    {
        g_current_led_status = LED_NULL;
    }
    
    return;
}

void oa_send_cced(void)
{
    oa_at_cmd_demo("AT+CCED=0\r\n");
    oa_at_cmd_demo_submit();
}

void oa_feed_watchdog(void *param)
{
    static oa_bool flag = OA_TRUE;
    
    if (flag == OA_TRUE)
    {
        oa_gpio_write(GPIO_HIGH, GPIO_PIN_WDT);
        flag = OA_FALSE;
    }
    else
    {
        oa_gpio_write(GPIO_LOW, GPIO_PIN_WDT);
        flag = OA_TRUE;
    }
    
    oa_timer_start(OA_TIMER_ID_6, oa_feed_watchdog, NULL, 600);
}

void oa_agps_connect(void)
{
    //oa_int8 sockID = 0;
    oa_int8 ret = 0;
    oa_sockaddr_struct AGPS_ADDR = {OA_SOCK_DGRAM,
                                    15,
                                    "agps.u-blox.com",
                                    46434};

    g_agps_socketID = oa_soc_create(OA_SOCK_DGRAM, 0);

    if (g_agps_socketID < 0)
    {
        APP_Debug("create UDP failed");
        return;
    }

    ret = oa_soc_connect(g_agps_socketID, OA_DOMAIN_NAME, &AGPS_ADDR);
        
    if(ret == OA_SOC_WOULDBLOCK)
    {  
        APP_Debug("UDP connect block");
    }
    else if(ret == OA_SOC_SUCCESS)
    {
        APP_Debug("UDP connect success");
    }
    else
    {
        APP_Debug("UDP connect failed");
        return;
    }
}

void oa_gps_handler(void)
{
    #if 0
    oa_uint8 GPS_CFG[] = {0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x04, 0x02, 0x00,
                          0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0x96, 0x00,
                          0x96, 0x00, 0x32, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0xFC};
    oa_uint8 GPS_SAVE[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1B, 0xA9};
    #endif
    
    static oa_bool gps_start = OA_FALSE;
    //static oa_bool gps_config = OA_FALSE;
    
    if (LOGIN_SUCCESS == g_LoginStatus)
    {
        //g_gps_count++;
        
        if (gps_start == OA_FALSE)
        {
            if (OA_FALSE == g_gps_poweron)
            {
                GPS_Power(ON);                
                oa_agps_connect();
                g_gps_poweron = OA_TRUE;
            }
            
            #if 0
            if ((gps_config == OA_FALSE)&&(g_gps_count == 5))
            {
                //APP_Debug("GPS CFG");
                //oa_uart_write(OA_UART2, GPS_CFG, sizeof(GPS_CFG));
                //oa_uart_write(OA_UART2, GPS_SAVE, sizeof(GPS_SAVE));
                //gps_config = OA_TRUE;
            }
            #endif
            
            if ((1 == g_Str_GPS_GPGGA.validity)||(2 == g_Str_GPS_GPGGA.validity))
            {
                //if (g_Str_GPS_GPGGA.satelliteSUM > 3)
                //{
                    g_locate_count = 0;
                    gps_start = OA_TRUE;
                //}
            }
        }
        else if (gps_start == OA_TRUE)
        {
            if (g_lowpopwer_sw == OA_FALSE)
            {
                if (g_oa_wristband_parameter.period >= 60)
                {
                    main_loop = 0;
                    g_main_period = 1;
                    g_lowpopwer_sw = OA_TRUE;
                }
            }
            else
            {
                if (g_oa_wristband_parameter.period < 60)
                {
                    main_loop = 0;
                    g_main_period = 1;
                    g_lowpopwer_sw = OA_FALSE;
                }
            }
            
            if (g_locate_count >= 40)//need test
            {   
                APP_Debug("restart GPS");
                oa_enable_low_power_sleep(OA_FALSE);
                gps_start = OA_FALSE;
                g_locate_count = 0;
                g_gps_poweron = OA_FALSE;
                g_lowpopwer_sw = OA_FALSE;
                g_main_period = 1;
                g_Str_GPS_GPGGA.validity = 0;
                g_Str_GPS_GPGGA.satelliteSUM = 0;
            }
        }
    }
    else
    {
        if ((OA_TRUE == g_gps_poweron)&&(g_lowpopwer_sw == OA_FALSE))
        {
            APP_Debug("else gps down");
            GPS_Power(DOWN);
            g_gps_poweron = OA_FALSE;
            g_Str_GPS_GPGGA.validity = 0;
        }
    }
}

void oa_restart_clear(void)
{
    main_loop       = 0;
    g_error_count   = 0;
    g_main_period   = 1;
    g_gps_poweron   = OA_FALSE;
    g_Alarm_Mode    = Alarm_NULL;
    g_LoginStatus   = LOGIN_CONNECT;
    
    GPS_Power(DOWN);
}

void oa_login_state_check(void)
{
    if (LOGIN_CONNECT == g_LoginStatus)
    {
        APP_Debug("LOGIN_CONNECT");
        oa_login();
        g_LoginStatus = LOGIN_BLOCK;
    }
    else if (LOGIN_BLOCK == g_LoginStatus)
    {
        g_error_count++;
    }
    else if (LOGIN_SUCCESS == g_LoginStatus)
    {
        if (g_Alarm_Mode != Alarm_NULL)
        {
            oa_alarm_check();
        }
        
        if ((g_havedata1 == 1)&&(g_havedata2 != 2))
        {
            g_havedata1 = 2;
            g_current_gps_data_file = oa_gps_data1;
            APP_Debug("oa_location_data_check1");
            oa_timer_start(OA_TIMER_ID_5, oa_location_data_check, NULL, 1000);
        }
        else if ((g_havedata2 == 1)&&(g_havedata1 != 2))
        {
            g_havedata2 = 2;
            g_current_gps_data_file = oa_gps_data2;
            APP_Debug("oa_location_data_check2");
            oa_timer_start(OA_TIMER_ID_5, oa_location_data_check, NULL, 1000);
        }
    }
}

void oa_error_check(void)
{
    if (g_error_count >= 60)
    {
        g_error_count = 0;
        
        if (LOGIN_BLOCK == g_LoginStatus)
        {
            APP_Debug("LOGIN_CONNECT OVERTIME, ready to reconnect");
            g_LoginStatus = LOGIN_CONNECT;
        }
        else
        {
    		APP_Debug("oa_module_restart!");
            oa_restart_clear();
            oa_module_restart(NULL);
        }
    }
}

void oa_enable_charger(oa_bool mode)
{
    if (OA_TRUE == mode)
    {
        oa_gpio_write(GPIO_HIGH, GPIO_PIN_CHARGE_ENABLE);
    }
    else
    {
        oa_gpio_write(GPIO_LOW, GPIO_PIN_CHARGE_ENABLE);
    }
}

void oa_lowpower_handler(void)
{
    static oa_uint8 AGPS_Count  = 0;
    
    //if located by GPS, then open lowpower switch
    if (g_lowpopwer_sw == OA_TRUE)
    {
        if (main_loop == 1)
        {
            APP_Debug("1");
            //g_main_period = 1;
            //oa_enable_low_power_sleep(OA_FALSE);
    
            if (OA_FALSE == g_gps_poweron)
            {
                APP_Debug("GPS on");
                GPS_Power(ON);
                
                if (AGPS_Count++ >= 10)
                {
                    oa_agps_connect();
                    AGPS_Count = 0;
                }
                g_gps_poweron = OA_TRUE;
            }
        }
        else if (main_loop == 12)
        {
            APP_Debug("12");

            if ((g_Str_GPS_GPGGA.validity != 1)&&(g_Str_GPS_GPGGA.validity != 2))
            {
                APP_Debug("APP CCED");
                oa_send_cced();
            }
        }
        else if (main_loop == 14)
        {
            APP_Debug("14");
            oa_location();
        }
        else if (main_loop == 16)
        {
            APP_Debug("16");
            //main_loop = 0;
    
            if (OA_TRUE == g_gps_poweron)
            {
                APP_Debug("GPS down");
                GPS_Power(DOWN);                
                g_gps_poweron = OA_FALSE;
                g_Str_GPS_GPGGA.validity = 0;
            }
            
            //>=60s&&not charging, then go into lowpower mode
            if (  (g_oa_wristband_parameter.period >= 60)
                &&(oa_battery_is_charger_connected() == OA_FALSE)
                &&(g_oa_wristband_parameter.mode == 0))
            {
                APP_Debug(">=60");
                oa_current_led(LED_NULL);//
                oa_soc_close_req();
                oa_enable_low_power_sleep(OA_TRUE);
                g_main_period = g_oa_wristband_parameter.period - 16;
            }
            else//normal mode
            {
                APP_Debug("else");
                g_main_period = 1;
            }
        }
        else if (main_loop > 16)
        {
            if (  (oa_battery_is_charger_connected() == OA_FALSE)
                &&(g_oa_wristband_parameter.mode == 0))
            {
                main_loop = 0;
            }
            else//charging wait for period
            {
                if (main_loop >= g_oa_wristband_parameter.period)
                {
                    main_loop = 0;
                }
            }
        }
    }
    else
    {
        if (main_loop == g_oa_wristband_parameter.period - 2)
        {
            oa_send_cced();
        }
        else if (main_loop >= g_oa_wristband_parameter.period)
        {
            main_loop = 0;
            oa_location();
        }
    }
}

void oa_app_scheduler_entry(void* param)
{
    static oa_bool first_valid  = OA_FALSE;
    static oa_bool GPS_Count    = OA_FALSE;

    if (OA_FALSE == g_upgrade)
    {
        if(oa_sim_network_is_valid())
    	{
    		if(!first_valid)
    		{
    			APP_Debug("GSM network init finished!");
    			oa_sms_init();
    			oa_soc_init();
                oa_sms_initial_by_at_cmd();
                ftp_init();

                first_valid = OA_TRUE;
    		}

            if(g_soc_context.state == OA_SOC_STATE_OFFLINE)
            {
                main_loop = 0;
                g_main_period = 1;
                oa_enable_low_power_sleep(OA_FALSE);
                
                if (oa_soc_connect_req() == OA_SOC_ERROR)
                    g_error_count++;
            }
            else if(g_soc_context.state == OA_SOC_STATE_CONNECT)
            {
            }
            else if(g_soc_context.state == OA_SOC_STATE_ONLINE)
            {
                main_loop++;
                APP_Debug("main_loop = %d", main_loop);
                oa_login_state_check();
                oa_lowpower_handler(); 
            }
    	}
        else
        {
            g_error_count++;
        }
        
        oa_gps_handler();
        oa_led_check();
        oa_adc_voltage_check();
    }
    else
    {
        g_error_count++;
    }
    oa_error_check();
    
    oa_timer_start(OA_TIMER_ID_1, oa_app_scheduler_entry, NULL, g_main_period*1000);
}

void oa_app_uart2_recv(void *param, oa_uint32 len)
{
    oa_uint8 i = 0;
    oa_uint8 switchtoNMEA[] = {0xA0, 0xA2, 0x00, 0x18, 
                               0x81, 0x02, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x01,
                               0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x25, 0x80,
                               0x01, 0x35, 0xB0, 0xB3};
    oa_char  pBuf[100]  = {0x00};
    oa_char *pHead = (oa_char *)param;
    oa_char *pTail = (oa_char *)param;
    
    //APP_Debug("%s:%d->%s", __func__, len, (oa_char *)pHead);
    
    if (!oa_strncmp(pHead, "$", 1))
    {
        //APP_Debug("%s:%d->%s", __func__, len, (oa_char *)pHead);

        while (1)
        {
            pTail = oa_strchr(pHead, '*');

            if (pTail == NULL)
            {
                //APP_Debug("pTail return");
                return;
            }
            else
            {
                oa_memset(pBuf, 0x00, sizeof(pBuf));
                oa_memcpy(pBuf, pHead, pTail - pHead);
                //pHead = pTail;
            }
            
            if((!oa_strncmp(pBuf, "$GPGGA,", 7))||(!oa_strncmp(pBuf, "$GNGGA,", 7)))
            {
                #if 0
                if (2 == g_LoginStatus)
                {
                    if (g_gps_poweron != OA_TRUE)
                    {
                        g_gps_poweron = OA_TRUE;
                    }
                }
                else
                {
                    APP_Debug("$GPGGA GPS_Power DOWN");
                    GPS_Power(DOWN);
                    g_gps_poweron = OA_FALSE;
                    g_Str_GPS_GPGGA.validity = 0;
                }
                #endif
                
                g_GPS_CMD = GPS_CMD_GPGGA;
                GPS_Data_Handler((oa_char *)pBuf);
            }
            else if((!oa_strncmp(pBuf, "$GPGSA,", 7))||(!oa_strncmp(pBuf, "$GNGSA,", 7)))
            {
                g_GPS_CMD = GPS_CMD_GPGSA;
                GPS_Data_Handler((oa_char *)pBuf);
            }
            else if((!oa_strncmp(pBuf, "$GPVTG,", 7))||(!oa_strncmp(pBuf, "$GNVTG,", 7)))
            {
                g_GPS_CMD = GPS_CMD_GPVTG;
                GPS_Data_Handler((oa_char *)pBuf);
            }
            else if((!oa_strncmp(pBuf, "$GPGSV,", 7))||(!oa_strncmp(pBuf, "$GNGSV,", 7)))
            {
                //turn off $GPGSV data
                //oa_uart_write(OA_UART2, "$PSRF103,03,00,00,01*27\r\n", oa_strlen("$PSRF103,03,00,00,01*27\r\n"));
            }
            else if((!oa_strncmp(pBuf, "$GPRMC,", 7))||(!oa_strncmp(pBuf, "$GNRMC,", 7)))
            {
                //turn off $GPRMC data
                //oa_uart_write(OA_UART2, "$PSRF103,04,00,00,01*20\r\n", oa_strlen("$PSRF103,04,00,00,01*20\r\n"));
                //open $GPSVTG data
                //oa_uart_write(OA_UART2, "$PSRF103,05,00,01,01*20\r\n", oa_strlen("$PSRF103,05,00,01,01*20\r\n"));
            }
            
            pHead = oa_strchr(pTail, '$');

            if (pHead == NULL)
            {
                //APP_Debug("pHead return");
                return;
            }
        }
    }
}

/****************************************************************
*---user's app initalize and callback functions register
*****************************************************************/
void oa_app_init(void)
{
    if (OA_FALSE == oa_soc_setting_init(SETTING_SOC))
    {
        APP_Debug("oa_soc_setting_init(SETTING_SOC) failed");
    }
    
    if (OA_FALSE == oa_soc_setting_init(SETTING_WRISTBAND))
    {
        APP_Debug("oa_soc_setting_init(SETTING_WRISTBAND) failed");
    }
    
    oa_soc_init_fast();

    oa_at_init();

    oa_sms_initial();

    oa_cust_cmd_register(oa_app_execute_custom_at_cmd);

    oa_eint_init();

    if(oa_uart_open(OA_UART2, &g_uart2_port_setting))
    {
        APP_Debug("UART2 open OK!");
        oa_uart_register_callback(OA_UART2, oa_app_uart2_recv);
    }
    else
    {
        APP_Debug("UART2 open failed!");
    }
}

void oa_eint(void* param)
{
    if (g_DisassemblyStatus == 1)
    {
        oa_enable_low_power_sleep(OA_FALSE);
        if (g_main_period != 1)
        {
            OA_DEBUG_USER("oa_eint");//just for test, can not use it
            g_main_period = 1;
            oa_timer_stop(OA_TIMER_ID_1);
            oa_timer_start(OA_TIMER_ID_1, oa_app_scheduler_entry, NULL, 1000);
        }
    }
    
    oa_timer_start(OA_TIMER_ID_6, oa_eint, NULL, 1000);
}

void oa_app_main(void)
{
    APP_Debug("oa_app_main startup");

    oa_imei_init();
    oa_app_init();
    
    g_upgrade = OA_FALSE;
    //oa_gpio_write(GPIO_HIGH, GPIO_PIN_CWD);//start watchdog
    //oa_timer_start(OA_TIMER_ID_6, oa_feed_watchdog, NULL, 1000);
    
    oa_timer_start(OA_TIMER_ID_1, oa_app_scheduler_entry, NULL, 3000);
    oa_timer_start(OA_TIMER_ID_6, oa_eint, NULL, 4000);
}

void oa_app_hw_init(void)
{
    OA_GPIO_INIT();
}

/*Warning: NEVER, NEVER, NEVER Modify the compile options!!!*/
#pragma arm section code = "OA_USER_ENTRY"
oa_bool oa_user_dll_entry_dispatch_to_sys(oa_user_dll_dispatch_sys_struct *param)
{
    param->verSr = OA_VERSION_NO;
    param->main_entry = oa_app_main;
    param->hw_init = oa_app_hw_init;
    /*if do not want to check, put zero(0x00) here*/
    param->api_check_sum = 0;
    param->trace_level = g_oa_trace_level;
    param->trace_port = g_oa_trace_port;
    param->ps_port = g_oa_ps_port;
    param->tst_port = g_oa_tst_port;
}
#pragma arm section code

