#include "oa_type.h"
#include "oa_api.h"
#include "oa_soc.h"
#include "oa_location.h"
#include "oa_gps.h"
#include "oa_crc.h"
#include "oa_setting.h"
#include "oa_common.h"
#include "oa_at.h"
#include "oa_adc.h"

#define GPS_DATA_FILE1      L"gps_data1.ini"
#define GPS_DATA_FILE2      L"gps_data2.ini"
#define LBS_DATA_FILE       L"lbs_data.ini"
#define GPS_DATA_LENGTH 39
#define LBS_DATA_LENGTH_MAX 71

oa_uint8 g_havedata1 = 0;    //0:no data 1:have data 2:send data
oa_uint8 g_havedata2 = 0;    //0:no data 1:have data 2:send data
oa_uint8 g_oa_at_cced_NO = 0;
oa_uint8 g_lbs_pre_next_same = 0;   //0:different   1:same
oa_uint8 g_location[100] = {0};
oa_uint8 g_gps_data[GPS_DATA_LENGTH + 1] = {0};
//oa_int32 g_handle = 0;

oa_nw_info_location_info_struct g_CellTrack = {0};
oa_lbs_parameter g_oa_lbs_parameter_pre[6] = {0};
oa_lbs_parameter g_oa_lbs_parameter_next[6] = {0};
oa_storedata_mode g_current_gps_data_file = oa_gps_null;
oa_location_mode g_oa_location_mode = oa_location_active;

void oa_location_store_without_gprs(oa_uint8 len, oa_storedata_mode mode)
{
    oa_int32 handle, ret;
    oa_uint32 dummy_read, dummy_write;
    oa_uint32 size;

    if (oa_gps_data1 == mode)
    {
        OA_DEBUG_USER("oa_fopen GPS_DATA_FILE1");
        handle = oa_fopen(GPS_DATA_FILE1);
    }
    else if (oa_gps_data2 == mode)
    {
        OA_DEBUG_USER("oa_fopen GPS_DATA_FILE2");
        handle = oa_fopen(GPS_DATA_FILE2);
    }

    if(handle < 0)
    {
        if (oa_gps_data1 == mode)
        {
            handle = oa_fcreate(GPS_DATA_FILE1);
        }
        else if (oa_gps_data2 == mode)
        {
            handle = oa_fcreate(GPS_DATA_FILE2);
        }

        if(handle < 0)
        {
            OA_DEBUG_USER("oa_fcreate GPS_DATA_FILE failed");
            return;
        }

        ret = oa_fwrite(handle, &g_location, len, &dummy_write);

        if((ret < 0) || (dummy_write != len))
        {
            OA_DEBUG_USER("oa_fcreate oa_fwrite failed");    
            return;
        }
        OA_DEBUG_USER("oa_fcreate oa_fwrite ok");    
    }
    else
    {
        ret = oa_fseek(handle, 0, OA_FILE_END);
        ret = oa_fwrite(handle, &g_location, len, &dummy_write);
        OA_DEBUG_USER("oa_fopen oa_fwrite OK");

        //we treat this status is that flash is full, so need to seek point to the begin of file to overcome.
        if((ret < 0) || (dummy_write != len))
        {
            OA_DEBUG_USER("oa_fopen oa_fwrite failed");
            ret = oa_fseek(handle, 0, OA_FILE_BEGIN);

            if (ret < 0)
            {
                OA_DEBUG_USER("oa_fseek OA_FILE_BEGIN failed");
                return;
            }
            
            ret = oa_fwrite(handle, &g_location, len, &dummy_write);

            if((ret < 0) || (dummy_write != len))
            {
                OA_DEBUG_USER("oa_fseek OA_FILE_BEGIN oa_fwrite failed");
                return;
            }
        }
    }
    
    oa_fsize(handle, &size);
    OA_DEBUG_USER("oa_location_store_without_gprs size=%d", size);
    
    ret = oa_fclose(handle);
}

void oa_location_data_check(void *param)
{
    static oa_bool first = OA_FALSE;
    static oa_int32 handle, ret;
    oa_uint32 dummy_read, dummy_write;

    if (OA_FALSE == first)
    {
        OA_DEBUG_USER("oa_location_data_check OA_FALSE");
        if (oa_gps_data1 == g_current_gps_data_file)
        {
            handle = oa_fopen(GPS_DATA_FILE1);
        }
        else if (oa_gps_data2 == g_current_gps_data_file)
        {
            handle = oa_fopen(GPS_DATA_FILE2);
        }

        if(handle < 0)
        {
            OA_DEBUG_USER("oa_location_data_check open error");
            return;
        }
        first = OA_TRUE;
    }
    
    if ((OA_TRUE == first)&&(2 == g_LoginStatus))
    {
        OA_DEBUG_USER("oa_location_data_check OA_TRUE");
        ret = oa_fread(handle, &g_gps_data, sizeof(g_gps_data), &dummy_read);

        if((ret < 0)||(dummy_read != sizeof(g_gps_data))) 
        {
            first = OA_FALSE;
            ret = oa_fclose(handle);
            
            if (oa_gps_data1 == g_current_gps_data_file)
            {
                g_havedata1 = 0;
                oa_fdelete(GPS_DATA_FILE1);
                OA_DEBUG_USER("GPS_DATA_FILE1 delete");
            }
            else if (oa_gps_data2 == g_current_gps_data_file)
            {
                g_havedata2 = 0;
                oa_fdelete(GPS_DATA_FILE2);
                OA_DEBUG_USER("GPS_DATA_FILE2 delete");
            }
            
            g_current_gps_data_file = oa_gps_null;
            oa_timer_stop(OA_TIMER_ID_5);
            return;
        }
        OA_DEBUG_USER("oa_location_data_check send");

        oa_soc_send(g_soc_context.socket_id, g_gps_data, GPS_DATA_LENGTH);
    }
    
    oa_timer_start(OA_TIMER_ID_5, oa_location_data_check, NULL, 1000);
}

void oa_location(void)
{
    oa_uint8 n = 0;
    oa_uint8 i = 0;
    oa_uint8 NS = 0;    //0:s 1:n
    oa_uint8 EW = 0;    //0:w 1:e
    oa_uint8 signal = 0;
    oa_uint16 crcvalue = 0;
    oa_int32 temp = 0;
    oa_time_struct time = {0};

    //g_gps_count = 0;
    //g_LBS_Count = 0;    //very important->SYNC 
    
    oa_memset(g_location, 0x00, sizeof(g_location));
    g_location[n++] = 0xFF;
    g_location[n++] = 0xFF;
    g_location[n++] = 0x00; //length
    g_location[n++] = g_serial_number/256;
    g_location[n++] = g_serial_number%256;
    g_serial_number++;

    if (0x8001 == g_serial_number)
    {
        g_serial_number = 0;
    }
    
    oa_get_time(&time);

    if ((1 == g_Str_GPS_GPGGA.validity)||(2 == g_Str_GPS_GPGGA.validity))
    {
        //GPS
        if (oa_location_active == g_oa_location_mode)
        {
            g_location[n++] = 0x01;
            //OA_DEBUG_USER("oa_location_active");
        }
        else if (oa_location_passivity == g_oa_location_mode)
        {
            g_location[n++] = 0x81;
            g_oa_location_mode = oa_location_active;
            //OA_DEBUG_USER("oa_location_passivity");
        }

        g_location[n++] = time.nYear%0xFFF;
        g_location[n++] = time.nMonth;
        g_location[n++] = time.nDay;
        g_location[n++] = time.nHour;
        g_location[n++] = time.nMin;
        g_location[n++] = time.nSec;
        
        temp = g_Str_GPS_GPGGA.latitude_mm*10000;
        g_location[n++] = g_Str_GPS_GPGGA.latitude_dd;
        g_location[n++] = (oa_uint8)(temp/10000);
        g_location[n++] = (oa_uint8)(temp%10000/100);
        g_location[n++] = (oa_uint8)(temp%100);
        
        temp = g_Str_GPS_GPGGA.longitude_mm*10000;
        g_location[n++] = g_Str_GPS_GPGGA.longitude_ddd/100;
        g_location[n++] = g_Str_GPS_GPGGA.longitude_ddd%100;
        g_location[n++] = (oa_uint8)(temp/10000);
        g_location[n++] = (oa_uint8)(temp%10000/100);
        g_location[n++] = (oa_uint8)(temp%100);

        if ((g_Str_GPS_GPGGA.latitude_NS == 's')||(g_Str_GPS_GPGGA.latitude_NS == 'S'))
        {
            NS = 0x00;
        }
        else
        {
            NS = 0x01;
        }

        if ((g_Str_GPS_GPGGA.longitude_EW == 'w')||(g_Str_GPS_GPGGA.longitude_EW == 'W'))
        {
            EW = 0x00;
        }
        else
        {
            EW = 0x10;
        }
        
        g_location[n++] = NS + EW;
        g_location[n++] = g_Str_GPS_GPGGA.satelliteSUM;
        g_location[n++] = (oa_uint8)g_Str_GPS_GPGSA.PDOP;    //accuracy

        if (g_Str_GPS_GPGGA.elevation < 0)
        {
            g_location[n++] = 0x01;
        }
        else
        {
            g_location[n++] = 0x00;
        }
        temp = oa_fabs(g_Str_GPS_GPGGA.elevation)*10;
        g_location[n++] = (oa_uint8)(temp/256);
        g_location[n++] = (oa_uint8)(temp%256);

        temp = g_Str_GPS_GPVTG.rate2*10;
        g_location[n++] = (oa_uint8)(temp/10);
        g_location[n++] = (oa_uint8)(temp%10);

        temp = g_Str_GPS_GPVTG.angle1*100;
        g_location[n++] = (oa_uint8)(temp/256); //reserve
        g_location[n++] = (oa_uint8)(temp%256);
        
        g_location[n++] = g_BatteryPercent;
        
        signal = oa_network_get_signal_level(NULL);
        if ((0 == signal)||(99 == signal))
        {
            g_location[n++] = 0x00;
        }
        else if (signal <= 8)
        {
            g_location[n++] = 0x01;
        }
        else if (signal <= 16)
        {
            g_location[n++] = 0x02;
        }
        else if (signal <= 24)
        {
            g_location[n++] = 0x03;
        }
        else
        {
            g_location[n++] = 0x04;
        }
        
        g_location[n++] = 0x00;
        g_location[n++] = 0x00;
        
        g_location[2] = n + 4;
        crcvalue = u16CRC_Calc16(g_location, n);
        g_location[n++] = crcvalue/256;
        g_location[n++] = crcvalue%256;

        g_location[n++] = 0xEE;
        g_location[n++] = 0xEE;
        //g_location[2] = n; //length
        g_locate_count = 0;
        OA_DEBUG_USER("oa_location GPS");
    }
    else
    {
        //LBS
        if (oa_location_active == g_oa_location_mode)
        {
            g_location[n++] = 0x02;
            //OA_DEBUG_USER("oa_location_active");
        }
        else if (oa_location_passivity == g_oa_location_mode)
        {
            g_location[n++] = 0x82;
            g_oa_location_mode = oa_location_active;
            //OA_DEBUG_USER("oa_location_passivity");
        }

        g_location[n++] = time.nYear;
        g_location[n++] = time.nMonth;
        g_location[n++] = time.nDay;
        g_location[n++] = time.nHour;
        g_location[n++] = time.nMin;
        g_location[n++] = time.nSec;

        g_location[n++] = g_oa_at_cced_NO; //LAC sum
#if 0
        if (OA_TRUE == oa_check_lbs_change())
        {
            g_lbs_pre_next_same = 1;
        }
        else
        {
            g_lbs_pre_next_same = 0;
        }
#endif        
        //if get more than 3 LBS data, then update, otherwise use old LBS data, avoiding LBS locating error!!!
        if (g_oa_at_cced_NO > 3)
        {
            oa_save_lbs();
        }

        for (i = 0; i < g_oa_at_cced_NO; i++)
        {
            g_location[n++] = g_oa_lbs_parameter_pre[i].mcc/256;
            g_location[n++] = g_oa_lbs_parameter_pre[i].mcc%256;
            g_location[n++] = g_oa_lbs_parameter_pre[i].mnc;
            g_location[n++] = g_oa_lbs_parameter_pre[i].lac/256;
            g_location[n++] = g_oa_lbs_parameter_pre[i].lac%256;
            g_location[n++] = g_oa_lbs_parameter_pre[i].ci/256;
            g_location[n++] = g_oa_lbs_parameter_pre[i].ci%256;
            g_location[n++] = g_oa_lbs_parameter_pre[i].rssi + 110;
        }

        g_location[n++] = g_BatteryPercent;
        
        signal = oa_network_get_signal_level(NULL);
        if ((0 == signal)||(99 == signal))
        {
            g_location[n++] = 0x00;
        }
        else if (signal <= 8)
        {
            g_location[n++] = 0x01;
        }
        else if (signal <= 16)
        {
            g_location[n++] = 0x02;
        }
        else if (signal <= 24)
        {
            g_location[n++] = 0x03;
        }
        else
        {
            g_location[n++] = 0x04;
        }
        g_location[n++] = 0x00;
        g_location[n++] = 0x00;
        g_location[n++] = 0x00;
        g_location[n++] = 0x00;
        
        g_location[2] = n + 4; //length
        crcvalue = u16CRC_Calc16(g_location, n);
        g_location[n++] = crcvalue/256;
        g_location[n++] = crcvalue%256;

        g_location[n++] = 0xEE;
        g_location[n++] = 0xEE;
        //g_location[2] = n; //length
        
        g_oa_at_cced_NO = 0;
        g_locate_count++;
        OA_DEBUG_USER("oa_location LBS:%d", g_locate_count);
    }

    if (LOGIN_SUCCESS == g_LoginStatus)
    {
        //if in LBS mode, and LBS not change, do not send LBS data
        //if ( ((0x02 == g_location[5])||(0x82 == g_location[5])) && (1 == g_lbs_pre_next_same) )
        //{
        //    OA_DEBUG_USER("not send LBS SAME");
        //    g_serial_number--;
        //}
        //else
        //{
            OA_DEBUG_USER("send");
            oa_soc_send(g_soc_context.socket_id, g_location, n);
        //}
    }
    else
    {
        //only store GPS locate data
        if ((1 == g_Str_GPS_GPGGA.validity)||(2 == g_Str_GPS_GPGGA.validity))
        {
            OA_DEBUG_USER("store");
            if (g_havedata1 != 2)
            {
                g_havedata1 = 1;
                oa_location_store_without_gprs(n, oa_gps_data1);
            }
            else if (g_havedata2 != 2)
            {
                g_havedata2 = 1;
                oa_location_store_without_gprs(n, oa_gps_data2);
            }
        }
        else
        {
            OA_DEBUG_USER("not store");
            g_serial_number--;
        }
    }
    //_timer_start(OA_TIMER_ID_4, oa_location, NULL, g_oa_wristband_parameter.period*1000);
}

