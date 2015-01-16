#ifndef _OA_ALARM_H_
#define _OA_ALARM_H_

#if 1
#define ALARM_Debug  OA_DEBUG_USER
#else
#define ALARM_Debug
#endif

enum
{
    Alarm_NULL = 0,
    Alarm_Low_Battery,
    Alarm_Low_Battery_ShutDown,
    Alarm_Destroy,
    Alarm_Charging,
    Alarm_Stop_Charging
};

extern oa_uint8 g_Alarm_Mode;
extern oa_uint8 g_send_count;

extern void oa_alarm(oa_uint8 mode);
extern void oa_alarm_check(void);

#endif

