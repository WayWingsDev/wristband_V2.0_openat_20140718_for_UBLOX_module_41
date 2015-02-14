/*****************************************************************************
 *  Copyright Statement:
 *  --------------------
 *  MobileTech(Shanghai) Co., Ltd.
 *
 *****************************************************************************/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   oa_setting.c
 *
 * Project:
 * --------
 *   OPEN AT project
 *
 * Description:
 * ------------
 *   This module contains setting routines for OPENAT.
 *
 * Author:
 * -------
 *   .
 *
 ****************************************************************************/
#include "oa_type.h"
#include "oa_api.h"
#include "oa_setting.h"

oa_soc_set_parameter soc_cs =
{
    {"XMYN12345678913774650324\r\n"},    //dtuid
    {"221.6.21.50"},   //serve_ipaddr "221.6.21.50""221.226.102.18"
    {"cmt.3322.org"},   //serve_host_name
    28088, //port 1102 8899 28081 28088
    {"cmnet"},{"root"},{"root"},//apninfo
    OA_FALSE,//OA_FALSE:TCP OA_TRUE:UDP

    {0x00, 0x00}, //checktail
};

oa_wristband_parameter g_oa_wristband_parameter_default = 
{
    0,
    900,
    900,
    0,
    {"15195978536", "13004590503", "13913047719"},
    {"221.6.21.50"},
};

oa_soc_set_parameter g_soc_param = {0};
oa_wristband_parameter g_oa_wristband_parameter = {0};

oa_bool oa_soc_setting_init(oa_uint8 param)
{
    oa_int32 handle, ret;
    oa_uint32 dummy_read, dummy_write;

    if (param == SETTING_SOC)
    {
        handle = oa_fopen(OA_SOC_SETTING_FILE);
    }
    else if (param == SETTING_WRISTBAND)
    {
        handle = oa_fopen(OA_WRISTBAND_SETTING_FILE);
    }
    else
    {
        OA_DEBUG_USER("param error");
        return OA_FALSE;
    }
    
    if(handle < 0)
    {
        if (param == SETTING_SOC)
        {
            handle = oa_fcreate(OA_SOC_SETTING_FILE);

            if(handle < 0)
            {
                OA_DEBUG_USER("Create soc setting file failed!");
                return OA_FALSE;
            }

            ret = oa_fwrite(handle, &soc_cs, sizeof(soc_cs), &dummy_write);

            if((ret < 0)||(dummy_write != sizeof(soc_cs)))
            {
                OA_DEBUG_USER("Init soc setting file failed!");
                return OA_FALSE;
            }
            OA_DEBUG_USER("Create soc setting file ok!");    
        }
        else if (param == SETTING_WRISTBAND)
        {
            handle = oa_fcreate(OA_WRISTBAND_SETTING_FILE);

            if(handle < 0)
            {
                OA_DEBUG_USER("Create wristband setting file failed!");
                return OA_FALSE;
            }

            ret = oa_fwrite(handle, &g_oa_wristband_parameter_default, sizeof(g_oa_wristband_parameter_default), &dummy_write);

            if((ret < 0)||(dummy_write != sizeof(g_oa_wristband_parameter_default)))
            {
                OA_DEBUG_USER("Init wristband setting file failed!");
                return OA_FALSE;
            }
            OA_DEBUG_USER("Create wristband setting file ok!");    
        }

        ret = oa_fseek(handle, 0, OA_FILE_BEGIN);
    }

    if (param == SETTING_SOC)
    {
        ret = oa_fread(handle, &g_soc_param, sizeof(g_soc_param), &dummy_read);

        if((ret < 0)||(dummy_read != sizeof(soc_cs))) 
        {
            OA_DEBUG_USER("oa_fread error");
            return OA_FALSE;
        }
    }
    else if (param == SETTING_WRISTBAND)
    {
        ret = oa_fread(handle, &g_oa_wristband_parameter, sizeof(g_oa_wristband_parameter), &dummy_read);
        
        if((ret < 0)||(dummy_read != sizeof(g_oa_wristband_parameter_default))) 
        {
            OA_DEBUG_USER("oa_fread error");
            return OA_FALSE;
        }
    }
  
    ret = oa_fclose(handle);

    if (g_oa_wristband_parameter.mode != 0)
    {
        g_oa_wristband_parameter.mode = 0;
        
        if (OA_TRUE == oa_soc_setting_save(SETTING_WRISTBAND))
        {
            OA_DEBUG_USER("oa_soc_setting_save error");
            return OA_FALSE;
        }
    }

    return OA_TRUE;
}

oa_bool oa_soc_setting_save(oa_uint8 param)
{
    oa_int32 handle_get, ret_get;
    oa_uint32 dummy_read_get,dummy_write_get;

    if (param == SETTING_SOC)
    {
        handle_get = oa_fopen(OA_SOC_SETTING_FILE);
        ret_get = oa_fwrite(handle_get, &g_soc_param, sizeof(g_soc_param), &dummy_write_get);
    }
    else if (param == SETTING_WRISTBAND)
    {
        handle_get = oa_fopen(OA_WRISTBAND_SETTING_FILE);
        ret_get = oa_fwrite(handle_get, &g_oa_wristband_parameter, sizeof(g_oa_wristband_parameter), &dummy_write_get);
    }
    else
    {
        return OA_FALSE;
    }

    ret_get = oa_fclose(handle_get);

    return OA_TRUE;
}

