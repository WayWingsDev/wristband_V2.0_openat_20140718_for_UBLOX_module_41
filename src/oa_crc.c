#include "oa_type.h"
#include "oa_api.h"
#include "oa_soc.h"
#include "oa_crc.h"

oa_char g_IMEI[9] = {0};

oa_uint16 u16CRC_Calc16(const oa_uint8 *pu8Data, oa_int32 i32Len)
{
     oa_uint8 i;
     oa_uint16 u16CRC = 0;

    while(--i32Len >= 0)
    {
        i = 8;
        u16CRC = u16CRC ^ (((oa_uint16)*pu8Data++) << 8);

        do
        {
             if (u16CRC & 0x8000)
             {
                  u16CRC = u16CRC << 1 ^ 0x1021;
             }
             else
             {
                  u16CRC = u16CRC << 1;
             }
        }
        while(--i);
    }
    return u16CRC;
}

