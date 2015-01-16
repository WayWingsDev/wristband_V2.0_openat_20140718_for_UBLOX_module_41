#include "oa_type.h"
#include "oa_heartbeat.h"
#include "oa_api.h"
#include "oa_soc.h"
#include "oa_adc.h"
#include "oa_crc.h"
#include "oa_common.h"
#include "oa_location.h"
#include "oa_setting.h"

oa_uint8 g_HeartBeat[18] = {0};

void oa_heartbeat(void)
{
    oa_uint8 i = 0;
    oa_uint8 temp = 0;
    oa_uint16 crcvalue = 0;

    if (2 == g_LoginStatus)
    {
        oa_memset(g_HeartBeat, 0x00, sizeof(g_HeartBeat));
        
        g_HeartBeat[0] = 0xFF;
        g_HeartBeat[1] = 0xFF;
        g_HeartBeat[2] = 0x11;
        g_HeartBeat[3] = g_serial_number/256;
        g_HeartBeat[4] = g_serial_number%256;
        g_serial_number++;

        if (0x8001 == g_serial_number)
        {
            g_serial_number = 0;
        }
        
        g_HeartBeat[5] = 0x04;

        
        g_HeartBeat[6] = g_BatteryPercent;
        
        temp = oa_network_get_signal_level(NULL);

        if ((0 == temp)||(99 == temp))
        {
            g_GSMSignalLevel = 0x00;
        }
        else if (temp <= 8)
        {
            g_GSMSignalLevel = 0x01;
        }
        else if (temp <= 16)
        {
            g_GSMSignalLevel = 0x02;
        }
        else if (temp <= 24)
        {
            g_GSMSignalLevel = 0x03;
        }
        else
        {
            g_GSMSignalLevel = 0x04;
        }
        g_HeartBeat[7] = g_GSMSignalLevel;
        g_HeartBeat[8] = g_ChargeStatus;
        g_HeartBeat[9] = g_DisassemblyStatus;

        g_HeartBeat[10] = 0x00; //reserve
        g_HeartBeat[11] = 0x00;
        g_HeartBeat[12] = 0x00;
        
        crcvalue = u16CRC_Calc16(g_HeartBeat, 13);
        g_HeartBeat[13] = crcvalue/256;
        g_HeartBeat[14] = crcvalue%256;
        g_HeartBeat[15] = 0xEE;
        g_HeartBeat[16] = 0xEE;
        
        OA_DEBUG_USER("oa_heartbeat");
        
        oa_soc_send(g_soc_context.socket_id, g_HeartBeat, 17);
    }
    
    oa_timer_start(OA_TIMER_ID_3, oa_heartbeat, NULL, g_oa_wristband_parameter.Heartbeatperiod*1000);
}

