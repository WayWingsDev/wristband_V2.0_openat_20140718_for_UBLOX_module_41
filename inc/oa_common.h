#ifndef _OA_COMMON_H_
#define _OA_COMMON_H_

#define GPIO_MODE               0

#define GPIO_PIN_WDT            0
#define GPIO_PIN_BLUE           9
#define GPIO_PIN_RED            8
#define GPIO_PIN_CWD            5
#define GPIO_PIN_GPS_ONOFF      32
#define GPIO_PIN_CHARGE_ENABLE  61
#define GPIO_PIN_CHARGING       31
#define GPIO_PIN_CHARGING_FULL  30

#define GPIO_INPUT              0
#define GPIO_OUTPUT             1

#define GPIO_LOW                0
#define GPIO_HIGH               1

typedef enum
{
    LED_SW_NULL = 0,
    LED_RED,
    LED_BLUE,
}OA_LED_SW_ENUM;

typedef enum
{
    LED_NULL = 0,
    LED_RED_FLASH,
    LED_RED_ON,
    LED_BLUE_ON,
    LED_BLUE_FLASH,
}OA_LED_ENUM;

typedef enum
{
    LOGIN_NULL = 0,
    LOGIN_CONNECT,
    LOGIN_BLOCK,
    LOGIN_SUCCESS,
    LOGIN_FAILED,
}OA_LOGIN_ENUM;

extern oa_bool      g_upgrade;
extern oa_bool      g_gps_poweron;
extern oa_int8      g_agps_socketID;
extern oa_uint8     g_locate_count;
extern oa_uint8     g_GSMSignalLevel;
extern oa_uint8     g_ChargeStatus;
extern oa_uint8     g_DisassemblyStatus;
extern oa_uint8     g_LoginStatus;  //0:not login 1:login but no return or failed 2:login success
extern oa_uint8     g_versionH;
extern oa_uint8     g_versionL;
//extern oa_uint32    g_LBS_Count;
//extern oa_uint32    g_gps_count;
extern oa_uint16    g_serial_number;
extern oa_uint32    g_main_period;
extern oa_uint32    main_loop;
extern OA_LED_ENUM  g_current_led_status;

extern void oa_current_led(OA_LED_ENUM led);
extern void oa_send_cced(void);
extern oa_bool oa_IP_isValid(oa_char *ipaddr);
extern void oa_enable_charger(oa_bool mode);
extern void oa_app_scheduler_entry(void* param);

#endif

