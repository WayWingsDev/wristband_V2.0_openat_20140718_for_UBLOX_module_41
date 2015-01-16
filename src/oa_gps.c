/*
 ============================================================================
 Name        : gps.c
 Author      : Sundy
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "oa_gps.h"
#include "oa_type.h"
#include "oa_setting.h"
#include "oa_common.h"

/*
OA TRACE:oa_app_uart2_recv:160->$GPGGA,043727.138,3204.3526,N,11844.7258,E,1,03,50.0,17.9,M,5.0,M,,0000*68
$GPGSA,A,2,19,03,27,,,,,,,,,,50.0,50.0,3.2*3C
$GPVTG,47.43,T,,M,1.33,N,2.5,K,N*30
*/

Str_GPS_GPGGA g_Str_GPS_GPGGA = {0};
Str_GPS_GPGSA g_Str_GPS_GPGSA = {0};
Str_GPS_GPRMC g_Str_GPS_GPRMC = {0};
Str_GPS_GPVTG g_Str_GPS_GPVTG = {0};

unsigned char g_GPS_CMD = GPS_CMD_NULL;

static oa_bool GPS_Data_Parse(char *pData, unsigned char pLen, unsigned char *Data1, float *Data2)
{
    char buf[60] = {0};

    if (pData == NULL)
    {
        return OA_FALSE;
    }

    if (Data1 != NULL)
    {
        oa_memset(buf, 0, 60);
    	oa_memcpy(buf, pData, pLen);
        *Data1 = (unsigned char)oa_atoi(buf);

        return OA_TRUE;
    }
    else if (Data2 != NULL)
    {
        oa_memset(buf, 0, 60);
    	oa_memcpy(buf, pData, pLen);
        *Data2 = (float)oa_atof(buf);

        return OA_TRUE;
    }
    else
    {
        return OA_FALSE;
    }
}

/*
*pData 不包括$GPGGA,
*/
static oa_bool GPS_GPGGA_Parse(char *pData)
{
    char *pTemp = NULL;
    
    while (pData != NULL)
    {
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, 2, &g_Str_GPS_GPGGA.hour, NULL))
            {
                return OA_FALSE;
            }
            GPS_Debug("hour=%d", g_Str_GPS_GPGGA.hour);

            pData += 2;
            if (OA_FALSE == GPS_Data_Parse(pData, 2, &g_Str_GPS_GPGGA.minute, NULL))
            {
                return OA_FALSE;
            }
            GPS_Debug("minute=%d", g_Str_GPS_GPGGA.minute);

            pData += 2;
            if (OA_FALSE == GPS_Data_Parse(pData, 2, &g_Str_GPS_GPGGA.second, NULL))
            {
                return OA_FALSE;
            }
            GPS_Debug("second=%d", g_Str_GPS_GPGGA.second);
        }

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, 2, &g_Str_GPS_GPGGA.latitude_dd, NULL))
            {
                return OA_FALSE;
            }
            GPS_Debug("latitude_dd=%d", g_Str_GPS_GPGGA.latitude_dd);

            pData += 2;
            if (OA_FALSE == GPS_Data_Parse(pData, 7, NULL, &g_Str_GPS_GPGGA.latitude_mm))
            {
                return OA_FALSE;
            }
            GPS_Debug("latitude_mm=%.4f", g_Str_GPS_GPGGA.latitude_mm);
        }

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            g_Str_GPS_GPGGA.latitude_NS = pData[0];
            GPS_Debug("latitude_NS=%c", g_Str_GPS_GPGGA.latitude_NS);
        }

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, 3, &g_Str_GPS_GPGGA.longitude_ddd, NULL))
            {
                return OA_FALSE;
            }
            GPS_Debug("longitude_ddd=%d", g_Str_GPS_GPGGA.longitude_ddd);

            pData += 3;
            if (OA_FALSE == GPS_Data_Parse(pData, 7, NULL, &g_Str_GPS_GPGGA.longitude_mm))
            {
                return OA_FALSE;
            }
            GPS_Debug("longitude_mm=%.4f", g_Str_GPS_GPGGA.longitude_mm);
        }

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            g_Str_GPS_GPGGA.longitude_EW = pData[0];
            GPS_Debug("longitude_EW=%c", g_Str_GPS_GPGGA.longitude_EW);
        }

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, pTemp - pData, &g_Str_GPS_GPGGA.validity, NULL))
            {
                return OA_FALSE;
            }
            
            if (g_Str_GPS_GPGGA.validity != 0)
            {
                OA_DEBUG_USER("validity=%d", g_Str_GPS_GPGGA.validity);
            }
        }

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, pTemp - pData, &g_Str_GPS_GPGGA.satelliteSUM, NULL))
            {
                return OA_FALSE;
            }
            GPS_Debug("satelliteSUM=%d", g_Str_GPS_GPGGA.satelliteSUM);
        }

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');//水平精确度

        pData = pTemp + 1;
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, pTemp - pData, NULL, &g_Str_GPS_GPGGA.elevation))
            {
                return OA_FALSE;
            }
            GPS_Debug("elevation=%.4f", g_Str_GPS_GPGGA.elevation);
        }
        
        return OA_TRUE;
    }   
}

/*
*pData 不包括$GPGSA,
*/
static oa_bool GPS_GPGSA_Parse(char *pData)
{
    oa_uint8 i = 0;
    char *pTemp = NULL;
    //char *pTail = NULL;
    
    while (pData != NULL)
    {
        for (i = 0; i < 14; i++)
        {
            pTemp = oa_strchr(pData, ',');
            pData = pTemp + 1;
        }
        
        pTemp = oa_strchr(pData, ',');
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, pTemp - pData, NULL, &g_Str_GPS_GPGSA.PDOP))
            {
                return OA_FALSE;
            }
        }
        GPS_Debug("PDOP=%.4f", g_Str_GPS_GPGSA.PDOP);
        
        return OA_TRUE;
    }   
}

/*
*pData 不包括$GPVTG,
*/
static oa_bool GPS_GPVTG_Parse(char *pData)
{
    oa_uint8 i = 0;
    char *pTemp = NULL;
    //char *pTail = NULL;
    
    while (pData != NULL)
    {
        pTemp = oa_strchr(pData, ',');
        
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, pTemp - pData, NULL, &g_Str_GPS_GPVTG.angle1))
            {
                return OA_FALSE;
            }
        }
        GPS_Debug("angle1=%.2f", g_Str_GPS_GPVTG.angle1);
        
        pData = pTemp + 1;
        
        for (i = 0; i < 5; i++)
        {
            pTemp = oa_strchr(pData, ',');
            pData = pTemp + 1;
        }
        
        pTemp = oa_strchr(pData, ',');
        
        if (pTemp != NULL)
        {
            if (OA_FALSE == GPS_Data_Parse(pData, pTemp - pData, NULL, &g_Str_GPS_GPVTG.rate2))
            {
                return OA_FALSE;
            }
        }
        GPS_Debug("rate2=%.4f", g_Str_GPS_GPVTG.rate2);
        
        return OA_TRUE;
    }   
}

oa_bool GPS_Data_Handler(char *pData)
{
    if (GPS_CMD_NULL == g_GPS_CMD)
    {
        //return OA_FALSE;
    }
    else
    {
        pData += 7;
        switch (g_GPS_CMD)
        {
            case GPS_CMD_GPGGA:
                if (OA_TRUE == GPS_GPGGA_Parse(pData))
                {
                    return OA_TRUE;
                }
                break;

            case GPS_CMD_GPGSA:
                if (OA_TRUE == GPS_GPGSA_Parse(pData))
                {
                    return OA_TRUE;
                }
                break;

            case GPS_CMD_GPGSV:
                break;

            case GPS_CMD_GPRMC:
                break;

            case GPS_CMD_GPVTG:
                if (OA_TRUE == GPS_GPVTG_Parse(pData))
                {
                    return OA_TRUE;
                }
                break;

            default:
                break;
        }
    }
    
    return OA_FALSE;
}

void GPS_Power(ENUM_GPS_POWER mode)
{
    if (mode == ON)
    {
        oa_gpio_write(GPIO_HIGH, GPIO_PIN_GPS_ONOFF);
    }
    else if (mode == DOWN)
    {
        oa_gpio_write(GPIO_LOW, GPIO_PIN_GPS_ONOFF);
    }
}

