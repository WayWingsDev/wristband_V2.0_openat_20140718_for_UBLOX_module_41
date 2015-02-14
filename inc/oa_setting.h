#ifndef _OA_SETTING_H_
#define _OA_SETTING_H_

#define SET_IPADDR_LEN      50
#define SET_HOST_NAME_LEN   50
#define SET_APNINFO_LEN     40
#define SET_APNNAME_LEN     16
#define SET_APNPASSWORD_LEN 16
#define SET_HOST_DNS_LEN    20
#define PHONE_NUMBER_LEN    12
#define FTP_ADDR_LEN        13

#define OA_SETTING_CNXT_SIZE (sizeof(oa_soc_set_parameter) - sizeof(oa_uint8)*2)//without checktail
#define OA_SETTING_CNXT_TOTAL_SIZE (sizeof(oa_soc_set_parameter))//with checktail

#define OA_SOC_SETTING_FILE L"soc_setting.ini"
#define OA_WRISTBAND_SETTING_FILE L"wristband_setting.ini"

enum
{
    SETTING_SOC = 0,
    SETTING_WRISTBAND
};

typedef struct
{
    oa_uint8 dtuid[26+1];//ID
    
    oa_char serve_ipaddr[SET_IPADDR_LEN+1];//service ip address
    oa_char serve_host_name[SET_HOST_NAME_LEN+1];//service host name if no ip address
    oa_int16 port;//service port
    oa_char apninfo[SET_APNINFO_LEN+1];//
    oa_char apnname[SET_APNNAME_LEN+1];
    oa_char apnpassword[SET_APNPASSWORD_LEN+1];
    oa_bool connct_type; //TRUE = TCP, FALSE = UDP

    oa_uint8 checktail[2];
}oa_soc_set_parameter;

typedef struct
{
    oa_uint8 mode;
    oa_uint32 period;
    oa_uint32 Heartbeatperiod;
    oa_uint8 alarm;
    oa_uint8 phonenumber[3][PHONE_NUMBER_LEN];
    oa_uint8 ftp[FTP_ADDR_LEN];
}oa_wristband_parameter;

extern oa_soc_set_parameter g_soc_param;
extern oa_wristband_parameter g_oa_wristband_parameter;

extern oa_bool oa_soc_setting_init(oa_uint8 param);
extern oa_bool oa_soc_setting_save(oa_uint8 param);

#endif/*_OA_SETTING_H_*/
