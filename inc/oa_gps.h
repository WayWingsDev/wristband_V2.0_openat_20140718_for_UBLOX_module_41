/*
 ============================================================================
 Name        : gps.h
 Author      : Sundy
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#ifndef _OA_GPS_H_
#define _OA_GPS_H_

#include "oa_type.h"
#include "oa_api.h"

enum
{
    GPS_CMD_NULL = 0,
    GPS_CMD_GPGGA,
    GPS_CMD_GPGSA,
    GPS_CMD_GPGSV,
    GPS_CMD_GPRMC,
    GPS_CMD_GPVTG,
    GPS_CMD_GPGLL,
    GPS_CMD_GPZDA
};

typedef enum
{
    DOWN = 0,
    ON
}ENUM_GPS_POWER;

typedef struct
{
    unsigned char   hour;
    unsigned char   minute;
    unsigned char   second;
    unsigned char   latitude_dd;
    float           latitude_mm;
    unsigned char   latitude_NS;
    unsigned char   longitude_ddd;
    float           longitude_mm;
    unsigned char   longitude_EW;
    unsigned char   validity;       //0:��λ��Ч 1:��λ��Ч
    unsigned char   satelliteSUM;   //�������� 00~12
    float           elevation;      //�����뺣ƽ��ĸ߶� (-9999.9~9999.9)
    unsigned int    checksum;       //У���
}Str_GPS_GPGGA;

typedef struct
{
    unsigned char   mode;           //M:manual A:automatic
    unsigned char   locatemode;     //1:δ��λ2:��ά��λ3:��ά��λ
    float           PDOP;           //�ۺ�λ�þ������ӣ�0.5-99.9��
    float           HDOP;           //ˮƽ�������ӣ�0.5-99.9��
    float           VDOP;           //��ֱ�������ӣ�0.5-99.9��
    unsigned int    checksum;       //У���
}Str_GPS_GPGSA;

typedef struct
{
    unsigned char   SNR;            //�����
    unsigned int    checksum;       //У���
}Str_GPS_GPGSV;

typedef struct
{
    unsigned char   hour;           //UTC ʱ��(ʱ)
    unsigned char   minute;         //��
    unsigned char   second;         //��
    unsigned char   validity;       //A:��Ч��λ V:��Ч��λ
    unsigned char   latitude_dd;    //γ��(��)
    float           latitude_mm;    //γ��(��)
    unsigned char   latitude_NS;    //γ�Ȱ���
    unsigned char   longitude_ddd;  //����(��)
    float           longitude_mm;   //����(��)
    unsigned char   longitude_EW;   //���Ȱ���
    float           rate;           //��������(000.0~999.9��)
    float           heading;        //���溽��(000.0~359.9��)
    unsigned char   day;            //UTCʱ��(��)
    unsigned char   month;          //��
    unsigned int    year;           //��
    float           magnetic_variation;//��ƫ��(000.0~180.0��)
    unsigned char   declination;    //��ƫ�Ƿ���(E:�� W:��)
    unsigned char   mode_indicator; //A:������λD:���E:����N:������Ч
    unsigned int    checksum;       //У���
}Str_GPS_GPRMC;

typedef struct
{
    float           angle1;         //�汱����ϵ�˶��Ƕ�(000~359��)
    float           angle2;         //�ű�����ϵ�˶��Ƕ�(000~359��)
    float           rate1;          //��������(000.0~999.9��)
    float           rate2;          //��������(0000.0~1851.8 ����/Сʱ)
    unsigned char   mode_indicator; //A:������λD:���E:����N:������Ч
    unsigned int    checksum;       //У���
}Str_GPS_GPVTG;

typedef struct
{
    unsigned int    checksum;       //У���
}Str_GPS_GPGLL;

typedef struct
{
    unsigned int    checksum;       //У���
}Str_GPS_GPZDA;

#if 0
#define GPS_Debug  OA_DEBUG_USER
#else
#define GPS_Debug
#endif

extern unsigned char g_GPS_CMD;

extern Str_GPS_GPGGA g_Str_GPS_GPGGA;
extern Str_GPS_GPGSA g_Str_GPS_GPGSA;
extern Str_GPS_GPRMC g_Str_GPS_GPRMC;
extern Str_GPS_GPVTG g_Str_GPS_GPVTG;

extern oa_bool GPS_Data_Handler(char *pData);
extern void GPS_Power(ENUM_GPS_POWER mode);

#endif

