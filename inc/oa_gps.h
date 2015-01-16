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
    unsigned char   validity;       //0:定位无效 1:定位有效
    unsigned char   satelliteSUM;   //卫星数量 00~12
    float           elevation;      //天线离海平面的高度 (-9999.9~9999.9)
    unsigned int    checksum;       //校验和
}Str_GPS_GPGGA;

typedef struct
{
    unsigned char   mode;           //M:manual A:automatic
    unsigned char   locatemode;     //1:未定位2:二维定位3:三维定位
    float           PDOP;           //综合位置精度因子（0.5-99.9）
    float           HDOP;           //水平精度因子（0.5-99.9）
    float           VDOP;           //垂直精度因子（0.5-99.9）
    unsigned int    checksum;       //校验和
}Str_GPS_GPGSA;

typedef struct
{
    unsigned char   SNR;            //信噪比
    unsigned int    checksum;       //校验和
}Str_GPS_GPGSV;

typedef struct
{
    unsigned char   hour;           //UTC 时间(时)
    unsigned char   minute;         //分
    unsigned char   second;         //秒
    unsigned char   validity;       //A:有效定位 V:无效定位
    unsigned char   latitude_dd;    //纬度(度)
    float           latitude_mm;    //纬度(分)
    unsigned char   latitude_NS;    //纬度半球
    unsigned char   longitude_ddd;  //经度(度)
    float           longitude_mm;   //经度(分)
    unsigned char   longitude_EW;   //经度半球
    float           rate;           //地面速率(000.0~999.9节)
    float           heading;        //地面航向(000.0~359.9度)
    unsigned char   day;            //UTC时间(日)
    unsigned char   month;          //月
    unsigned int    year;           //年
    float           magnetic_variation;//磁偏角(000.0~180.0度)
    unsigned char   declination;    //磁偏角方向(E:东 W:西)
    unsigned char   mode_indicator; //A:自主定位D:差分E:估算N:数据无效
    unsigned int    checksum;       //校验和
}Str_GPS_GPRMC;

typedef struct
{
    float           angle1;         //真北参照系运动角度(000~359度)
    float           angle2;         //磁北参照系运动角度(000~359度)
    float           rate1;          //地面速率(000.0~999.9节)
    float           rate2;          //地面速率(0000.0~1851.8 公里/小时)
    unsigned char   mode_indicator; //A:自主定位D:差分E:估算N:数据无效
    unsigned int    checksum;       //校验和
}Str_GPS_GPVTG;

typedef struct
{
    unsigned int    checksum;       //校验和
}Str_GPS_GPGLL;

typedef struct
{
    unsigned int    checksum;       //校验和
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

