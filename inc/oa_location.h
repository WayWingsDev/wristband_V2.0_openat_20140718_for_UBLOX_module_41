#ifndef _OA_LOCATION_H_
#define _OA_LOCATION_H_

typedef enum
{
    oa_location_active = 0, //主动上传
    oa_location_passivity   //被动点名
}oa_location_mode;

typedef enum
{
    oa_gps_null = 0,
    oa_gps_data1,
    oa_gps_data2
}oa_storedata_mode;

typedef struct
{
    oa_uint32 mcc;
    oa_uint8 mnc;
    oa_uint16 lac;
    oa_uint16 ci;
    oa_uint8 rssi;
}oa_lbs_parameter;

extern oa_uint8 g_havedata1;    //0:no data 1:have data 2:send data
extern oa_uint8 g_havedata2;    //0:no data 1:have data 2:send data
extern oa_uint8 g_oa_at_cced_NO;
extern oa_uint8 g_lbs_pre_next_same;   //0:different   1:same
extern oa_uint8 g_location[100];
extern oa_lbs_parameter g_oa_lbs_parameter_pre[6];
extern oa_lbs_parameter g_oa_lbs_parameter_next[6];
extern oa_storedata_mode g_current_gps_data_file;
extern oa_location_mode g_oa_location_mode;

extern void oa_location(void);
extern void oa_location_data_check(void *param);
extern void oa_location_store_without_gprs(oa_uint8 len, oa_storedata_mode mode);

#endif

