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
 *   oa_ftp.h
 *
 * Project:
 * --------
 *   OPEN AT project
 *
 * Description:
 * ------------
 *   This Module gives a sample app on open as ftp client.
 *
 * Author:
 * -------
 *   None!
 *
 ****************************************************************************/
#ifndef _OA_FTP_H_
#define _OA_FTP_H_


#include "oa_type.h"
#include "oa_api.h"


#define FTP_WAIT_DATA_DELAY (70000)
#define FTP_WAIT_CMD_RET_DELAY (45000)

#define MAX_NUM_OF_PARAM  16 
#define MAX_LEN_OF_PARAM  (184*2)   /*184 is the max length for command SMSW that pdu max length*/

#define FTP_IS_FIG(A) ((A)>='0'&&(A)<='9')

typedef enum
{
    FTP_STATE_NONE = 0,             // 0
    FTP_STATE_CREATE_CTRL_SOCK,    
    FTP_STATE_CREATE_DATA_SOCK,
    FTP_STATE_CONNECT_CTRL_PIPE,    // 3 
    FTP_STATE_CONNECT_DATA_PIPE,
    FTP_STATE_LOGINON_USER,
    FTP_STATE_LOGINON_PASS,
    FTP_STATE_TYPE,                 //7
    FTP_STATE_FILESIZE,
    FTP_STATE_PASV_MOD,
    FTP_STATE_FILE_REST,
    FTP_STATE_DLFILE_REQ,
    FTP_STATE_TRANSFER_GOING,   

    FTP_STATE_QUIT_LOGIN,           //13
    FTP_STATE_TOTAL
    
}FTP_STATE_ENUM;

typedef enum
{
    /*System control command*/
    FTP_CMD_NONE,
    FTP_CMD_START,
    FTP_CMD_SUCCESS,
    FTP_CMD_TIMEROUT,
    FTP_CMD_FAIL,

    /*FTP protocol command*/
    FTP_RET_CMD_DATA_CONNECT_ALREADY    = 125, /*Data connection already open; Transfer starting.*/
    FTP_RET_CMD_CONNECTED               = 220, /*service ready*/
    FTP_RET_CMD_REQ_PW                  = 331,
    FTP_RET_CMD_LOGGEDON                = 230,
    FTP_RET_CMD_CWD                     = 250,
    FTP_RET_CMD_TYPE                    = 200,
    FTP_RET_CMD_PASVMOD                 = 227,
    FTP_RET_CMD_DATA_PIPE_ACCEPT        = 150,
    FTP_RET_CMD_TRANSFER_OK             = 226,
    FTP_RET_CMD_FILESIZE                = 213,
    FTP_RET_CMD_REST                    = 350,
    FTP_RET_CMD_GOODBYE                 = 221,

    /*error*/
    /*421 is the most common error*/
    FTP_RET_CMD_SERVER_LOCKED           = 421, /**/
    FTP_RET_CMD_CONNECT_TIMEOUT         = 421, /*Connection timed out*/ 
    FTP_RET_CMD_BE_TICKED               = 421, /*be ticked*/
    
    FTP_RET_CMD_USER_PW_INCORRECT       = 530, /*Login or password incorrect*/
    FTP_RET_CMD_FILE_NOT_FIND           = 550, /*File not found*/
    FTP_RET_CMD_CLOSE_CONNECT           = 426, /*Connection closed; transfer aborted.*/

    //FTP_RET_NUM,
}FTP_CMD_ENUM;

typedef enum
{
	FTP_FLAG_NO_RSET    = 0x01,
	FTP_FLAG_NOT_DLL    = 0x02,
	FTP_FLAG_NOT_CRC    = 0x04,
	FTP_FLAG_EN_PCT     = 0x08, //enable download percentage
	FTP_FORCE_NEW_DLL   = 0x10,//force to create a new openatdll.dll.bak
	/*ADD other flag*/
	FTP_FLAG_MAX = 0xFF
}FTP_PARAM_FLAG_ENUM;


typedef struct
{
    FTP_STATE_ENUM state;
    
    oa_int8 ctrl_sock_handle;
    oa_int8 data_sock_handle;

    oa_char *cmdRetBuf;
    oa_int32 downloadFileHandle;
    oa_uint32 downloadFileSize;
    oa_uint32 downloadDataCount;/*data size that already download*/
    oa_int16 FatalErrorCount;

    oa_sockaddr_struct  serv_ctrl_addr;
    oa_sockaddr_struct  serv_data_addr;
    oa_socket_addr_enum serv_ctrl_addr_type;
    oa_socket_addr_enum serv_data_addr_type;
    oa_char ftp_username[16];
    oa_char ftp_password[16];
    oa_char downloadFileName[64];
    oa_char downloadFileNameFullpath[64];/*something like "ephemeris/Eph.dat"*/
    oa_char ftp_cmd[64];
	oa_uint32 ftp_flag;

    oa_int16 data_pipe_recon;
    oa_uint32 last_time_stamp;
    //oa_bool  noAutoRestart;  /*need restart or not after upgrade finished, default is 0 will auto restart*/
}ftp_context_struct;

typedef struct ftp_custom_cmd_param_struct_tag
{
    oa_uint8 numOfParam;
    oa_char paramStr[MAX_NUM_OF_PARAM][MAX_LEN_OF_PARAM+1]; 
}ftp_custom_cmd_param_struct;//never alloc it in stack

typedef struct
{
    oa_uint16 cmd; 
}ftp_cmd_req_struct;


//external functions:
oa_bool ftp_core_step_handler(oa_uint8 * data);
void ftp_init(void);
void ftp_app_socket_notify_ind(void *inMsg);


#endif /*_OA_FTP_H_*/
