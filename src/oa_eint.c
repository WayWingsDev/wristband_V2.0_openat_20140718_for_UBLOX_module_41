#include "oa_type.h"
#include "oa_api.h"
#include "oa_eint.h"
#include "oa_alarm.h"
#include "oa_setting.h"
#include "oa_common.h"

#define _charging_ 0

oa_bool sos_handle_polarity_0 = OA_TRUE;
oa_uint8 g_eint_valid = 0x00;

void oa_eint_handler(void)
{
    OA_DEBUG_USER("oa_eint_handler");//just for test, can not use it
    g_eint_valid = 1;
    g_DisassemblyStatus = 1;
    
    if (g_oa_wristband_parameter.alarm)
    {
        g_Alarm_Mode = Alarm_Destroy;
    }
}

void oa_eint_handler2(void)
{
#if _charging_
    OA_DEBUG_USER("oa_eint_handler2");//just for test, can not use it
    oa_enable_low_power_sleep(OA_FALSE);
    g_main_period = 1;
    main_loop = 0;
    oa_timer_start(OA_TIMER_ID_1, oa_app_scheduler_entry, NULL, 1000);
#endif
}

void oa_eint_init(void)
{
    oa_gpio_mode_setup(56, 3);
    oa_gpio_init(0, 56);
    oa_eint_set_sensitivity(7, 1);
    oa_eint_set_debounce(7, 200);
    oa_eint_registration(7, OA_TRUE, 0, oa_eint_handler, OA_TRUE);

    #if _charging_
    oa_gpio_mode_setup(5, 3);
    oa_gpio_init(0, 5);
    oa_eint_set_sensitivity(1, 1);
    oa_eint_set_debounce(1, 200);
    oa_eint_registration(1, OA_TRUE, 0, oa_eint_handler2, OA_TRUE);
    #endif
    
    OA_DEBUG_USER("oa_eint_init");
}

