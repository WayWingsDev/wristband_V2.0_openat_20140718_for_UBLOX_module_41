#include "oa_type.h"
#include "oa_api.h"
#include "oa_adc.h"
#include "oa_alarm.h"
#include "oa_gps.h"
#include "oa_common.h"

//#define ADC_SETTING_FILE L"adc_setting.ini"
#define ADC_VOLTAGE_NUMBER 6

oa_uint8 g_BatteryPercent = 0;
//oa_bool  g_LowBatShutDownGPS = OA_FALSE;
oa_uint32 adc_voltage = 0;
oa_int32 g_ADC_Voltage_Table[ADC_VOLTAGE_NUMBER] = {0};

//oa_adc_set_parameter g_adc_param_default = {2};
//oa_adc_set_parameter g_adc_param = {0};

void bubblesort(oa_int32 data[], oa_uint8 n)
{
    oa_uint8 i, j;
    oa_uint32 temp = 0;

    if ((data == NULL)||(0 == n))
    {
        return;
    }

    for (i = 0; i < n; i++)
    {
        for (j = 0; j < i; j++)
        {
            if (data[j] > data[i])
            {
                temp = data[j];
                data[j] = data[i];
                data[i] = temp;
            }
        }
    }
}

oa_int32 average(oa_int32 data[], oa_uint8 low, oa_uint8 high)
{
    oa_uint8 n = 0;
    oa_int32 sum = 0;

    if (data == NULL)
    {
        return;
    }
    else if (low == high)
    {
        return data[low];
    }

    for (n = low; n < high + 1; n++)
    {
        sum += data[n];
    }

    return sum/(high - low + 1);
}

void oa_battery_check(oa_uint32 adc_voltage)
{
    if (adc_voltage >= 4200000)
    {
        g_BatteryPercent = 100;
    }
    else if (adc_voltage >= 4060000)
    {
        g_BatteryPercent = 90;
    }
    else if (adc_voltage >= 3980000)
    {
        g_BatteryPercent = 80;
    }
    else if (adc_voltage >= 3920000)
    {
        g_BatteryPercent = 70;
    }
    else if (adc_voltage >= 3870000)
    {
        g_BatteryPercent = 60;
    }
    else if (adc_voltage >= 3820000)
    {
        g_BatteryPercent = 50;
    }
    else if (adc_voltage >= 3790000)
    {
        g_BatteryPercent = 40;
    }
    else if (adc_voltage >= 3770000)
    {
        g_BatteryPercent = 30;
    }
    else if (adc_voltage >= 3740000)
    {
        g_BatteryPercent = 20;
    }
    else if (adc_voltage >= 3680000)
    {
        g_BatteryPercent = 10;
        if (Alarm_NULL == g_Alarm_Mode)
        {
            g_Alarm_Mode = Alarm_Low_Battery;
        }
    }
    else if (adc_voltage >= 3450000)
    {
        g_BatteryPercent = 5;
        if (Alarm_NULL == g_Alarm_Mode)
        {
            g_Alarm_Mode = Alarm_Low_Battery_ShutDown;
        }
    }
    else
    {
        g_BatteryPercent = 0;
    }
}

void oa_adc_voltage_check(void)
{
    static oa_uint8 n = 0;
    static oa_bool charging = OA_FALSE;
        
    g_ADC_Voltage_Table[n] = oa_adc_get_vbat_voltage();
    if ((g_ADC_Voltage_Table[n] > 3300000)&&(g_ADC_Voltage_Table[n] < 4400000))
    {
        n++;
    }

    if (ADC_VOLTAGE_NUMBER == n)
    {
        bubblesort(g_ADC_Voltage_Table, ADC_VOLTAGE_NUMBER);
        adc_voltage = average(g_ADC_Voltage_Table, 1, ADC_VOLTAGE_NUMBER - 2);
        oa_battery_check(adc_voltage);
        n = 0;
        ADC_Debug("adc_voltage=%d", adc_voltage);
    }
  
    if (oa_battery_is_charger_connected() == OA_TRUE)
    {
        ADC_Debug("charging");
        
        if ((charging == OA_FALSE)&&(Alarm_NULL == g_Alarm_Mode))
        {
            g_Alarm_Mode = Alarm_Charging;
            charging = OA_TRUE;
        }
        
        if ((OA_TRUE == oa_is_charge())&&(OA_FALSE == oa_is_full()))
        {
            oa_current_led(LED_RED_ON);
        }
        else if((OA_FALSE == oa_is_charge())&&(OA_TRUE == oa_is_full()))
        {
            oa_current_led(LED_BLUE_ON);
        }

        g_ChargeStatus = 1;
        g_main_period = 1;
        oa_enable_charger(OA_TRUE);
        oa_enable_low_power_sleep(OA_FALSE);
    }
    else
    {
        if ((charging == OA_TRUE)&&(Alarm_NULL == g_Alarm_Mode))
        {
            g_Alarm_Mode = Alarm_Stop_Charging;
            charging = OA_FALSE;
        }

        if (g_BatteryPercent <= 10)
        {
            oa_current_led(LED_RED_FLASH);
        }
        else
        {
            oa_current_led(LED_NULL);
        }
        
        g_ChargeStatus = 0;
        oa_enable_charger(OA_FALSE);
    }
}

oa_bool oa_is_charge(void)
{
    if (0 == oa_gpio_read(GPIO_PIN_CHARGING))
    {
        return OA_TRUE;
    }

    return OA_FALSE;
}

oa_bool oa_is_full(void)
{
    if (0 == oa_gpio_read(GPIO_PIN_CHARGING_FULL))
    {
        return OA_TRUE;
    }
    
    return OA_FALSE;
}

