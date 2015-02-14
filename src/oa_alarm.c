#include "oa_type.h"
#include "oa_api.h"
#include "oa_soc.h"
#include "oa_crc.h"
#include "oa_alarm.h"
#include "oa_common.h"
#include "oa_location.h"

oa_uint8 g_Alarm[15] = {0};
oa_uint8 g_Alarm_Mode = Alarm_NULL;
oa_uint8 g_send_count = 58;

void oa_alarm(oa_uint8 mode)
{
    oa_uint8 i = 0;
    oa_uint16 crcvalue = 0;
    
    oa_memset(g_Alarm, 0x00, sizeof(g_Alarm));
    
    g_Alarm[0] = 0xFF;
    g_Alarm[1] = 0xFF;
    g_Alarm[2] = 0x0E;
    g_Alarm[3] = g_serial_number/256;
    g_Alarm[4] = g_serial_number%256;
    g_serial_number++;
    
    if (0x8001 == g_serial_number)
    {
        g_serial_number = 0;
    }
    
    g_Alarm[5] = 0x03;

    if (mode == Alarm_Low_Battery)
    {
        g_Alarm[6] = 0x01;
    }
    else if (mode == Alarm_Low_Battery_ShutDown)
    {
        g_Alarm[6] = 0x02;
    }
    else if (mode == Alarm_Destroy)
    {
        g_Alarm[6] = 0x03;
    }
    else if (mode == Alarm_Charging)
    {
        g_Alarm[6] = 0x04;
    }
    else if (mode == Alarm_Stop_Charging)
    {
        g_Alarm[6] = 0x05;
    }
    else
    {
        return;
    }
    
    g_Alarm[7] = 0x00; //reserved
    g_Alarm[8] = 0x00;
    g_Alarm[9] = 0x00;
    
    crcvalue = u16CRC_Calc16(g_Alarm, 10);
    g_Alarm[10] = crcvalue/256;
    g_Alarm[11] = crcvalue%256;
    g_Alarm[12] = 0xEE;
    g_Alarm[13] = 0xEE;

    if (++g_send_count >= 60)
    {
        oa_soc_send(g_soc_context.socket_id, g_Alarm, 14);
        g_send_count = 0;
    }
}

void oa_alarm_check(void)
{
    ALARM_Debug("Alarm:%d", g_Alarm_Mode);
    
    switch (g_Alarm_Mode)
    {
        case Alarm_Low_Battery:
            oa_alarm(Alarm_Low_Battery);
            break;
            
        case Alarm_Low_Battery_ShutDown:
            oa_alarm(Alarm_Low_Battery_ShutDown);
            break;
            
        case Alarm_Destroy:
            oa_alarm(Alarm_Destroy);
            break;
            
        case Alarm_Charging:
            oa_alarm(Alarm_Charging);
            break;
            
        case Alarm_Stop_Charging:
            oa_alarm(Alarm_Stop_Charging);
            break;

        default:
            break;
    }
}

