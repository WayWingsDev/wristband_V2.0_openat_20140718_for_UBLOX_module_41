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
 *   oa_global.h
 *
 * Project:
 * --------
 *   OPEN AT project
 *
 * Description:
 * ------------
 * 
 *
 * Author:
 * -------
 *   simon.
 *
 ****************************************************************************/

#ifndef _OA_GLOBAL_H_
#define _OA_GLOBAL_H_
/*
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
*/
//#define OA_APP_PROTOCOL_TRANS

/*CUSTOMER define */
#define OPENAT_CUSTOM_XXX  //XXX for customer

//#define OPENAT_LCD_SUPPORT  //openat SPI LCD support for customer
//#define OPENAT_LCD_DEMO

//#define OPENAT_SOC_DEMO    //if need use gprs,can open this macro for test 

#define FTP_CTRL_TIMER OA_TIMER_ID_10//for openat ftp downloading
#define FTP_DATA_TIMER OA_TIMER_ID_11//for openat ftp downloading

#endif /*_OA_GLOBAL_H_*/

