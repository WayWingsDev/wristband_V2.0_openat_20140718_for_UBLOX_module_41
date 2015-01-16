#ifndef _OA_ADC_H_
#define _OA_ADC_H_

#if 1
#define ADC_Debug  OA_DEBUG_USER
#else
#define ADC_Debug
#endif

#if 0
typedef struct
{
    oa_uint32 period;
}oa_adc_set_parameter;
#endif

extern oa_uint8 g_BatteryPercent;
extern oa_uint32 adc_voltage;
//extern oa_bool  g_LowBatShutDownGPS;

extern void bubblesort(oa_int32 data[], oa_uint8 n);
extern oa_int32 average(oa_int32 data[], oa_uint8 low, oa_uint8 high);
extern void oa_adc_voltage_check(void);
extern oa_bool oa_is_charge(void);
extern oa_bool oa_is_full(void);

#if 0
extern void oa_adc_setting_init(void);
extern void oa_adc_setting_save(void);
#endif

#endif
