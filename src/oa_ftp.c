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
 *   oa_ftp.c
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
#include "oa_global.h"
#include "oa_type.h"
#include "oa_api.h"

#include "oa_ftp.h"

#define FTP_CMD_STR_LEN 3

#define FTP_DLL_BAK_PATH L"C:\\openatdll.dll.bak" //for downloading temp file wide string 
#define FTP_DLL_PATH L"C:\\openatdll.dll" //wide string
#define MAX_FTP_NETWORK_BUFLEN  30000

const oa_uint8 FTP_DLL_NAME[] = "openatdll.dll";

/************ FOR FTP************/
ftp_context_struct g_ftp_context={0};
oa_uint8  g_ftp_rec_databuf[MAX_FTP_NETWORK_BUFLEN+1] = {0};  // 接收数据缓冲区, 30K.

//UART_PORT FTP_UART_PORT=0; /*AT command port*/
extern void ftp_send_cmd(oa_uint16 event );
extern void ftp_reset_global_cntx(ftp_context_struct *ftp_cntx);
extern void ftp_state_cmd_handle(FTP_CMD_ENUM cmd);

/*to store current ctrl/data pipe status*/
oa_int8 gFTPCurrConnectSockId = -1;
oa_int32 gFTPCurrConnectSockPort = 0;
oa_socket_type_enum gFTPSockType = OA_SOCK_STREAM; /*Used to keep the created socket type*/
oa_ftp_rsp_cb_fptr ftp_rsp_cb_callback= NULL;

oa_char ftp_state_enum_name_map[16][32] = 
{
    "FTP_STATE_NONE",
    "FTP_STATE_CREATE_CTRL_SOCK",
    "FTP_STATE_CREATE_DATA_SOCK",
    "FTP_STATE_CONNECT_CTRL_PIPE",
    "FTP_STATE_CONNECT_DATA_PIPE",
    "FTP_STATE_LOGINON_USER",
    "FTP_STATE_LOGINON_PASS",
    "FTP_STATE_TYPE",
    "FTP_STATE_FILESIZE",
    "FTP_STATE_PASV_MOD",
    "FTP_STATE_FILE_REST",
    "FTP_STATE_DLFILE_REQ",
    "FTP_STATE_TRANSFER_GOING",
    "FTP_STATE_QUIT_LOGIN",
    "FTP_STATE_TOTAL",
    "FTP_STATE_RESERVE",
};



//need put in the socket main notify indication
void ftp_ctrl_socket_notify_ind(oa_app_soc_notify_ind_struct *pMsg);
void ftp_data_socket_notify_ind(oa_app_soc_notify_ind_struct *pMsg);
void ftp_app_socket_notify_ind(void *inMsg)
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    oa_app_soc_notify_ind_struct *soc_notify = (oa_app_soc_notify_ind_struct*) inMsg;

    OA_DEBUG_L3("%s,socket_id=%d, event_type=%d,result=%d",
                                        __func__, 
                                        soc_notify->socket_id,
                                        soc_notify->event_type,
                                        soc_notify->result);
    /*----------------------------------------------------------------*/
    /* Code Body                                                      */
    /*----------------------------------------------------------------*/
    if(soc_notify->socket_id  == g_ftp_context.ctrl_sock_handle)
    { 
        ftp_ctrl_socket_notify_ind(soc_notify);
    }
    else 
    if(soc_notify->socket_id  == g_ftp_context.data_sock_handle)
    {
        ftp_data_socket_notify_ind(soc_notify);
    }
}

void ftp_receive_evshed_msg(void *param)
{
    ftp_cmd_req_struct *myMsgPtr = (ftp_cmd_req_struct *)param;

    if(myMsgPtr)
    {
        ftp_state_cmd_handle(myMsgPtr->cmd);
        oa_free(myMsgPtr);
    }
}

/*Send ftp cmd to ftp state machine*/
void ftp_send_cmd(oa_uint16 cmd)
{
    ftp_cmd_req_struct *myMsgPtr;

    /*----------------------------------------------------------------*/
    /* Code Body                                                      */
    /*----------------------------------------------------------------*/
    myMsgPtr = (ftp_cmd_req_struct*)oa_malloc(sizeof(ftp_cmd_req_struct));
    if(myMsgPtr)
    {
        oa_memset(myMsgPtr, 0x00, sizeof(ftp_cmd_req_struct));
        myMsgPtr->cmd = cmd;
        oa_evshed_start(OA_EVSHED_ID_0, ftp_receive_evshed_msg, (void *)myMsgPtr, 500);
    }
    else
    {
        OA_DEBUG_L3("Malloc failed!!! return!!!");
    }
}


/*****************************************************************************
* FUNCTION
*   ftp_custom_command_parse_param()
* DESCRIPTION
*    Parse AT command string to parameters 
* PARAMETERS
*   char* pStr
* RETURNS
*  pCmdParam
*****************************************************************************/
oa_bool ftp_custom_command_parse_param(oa_char* pStr, ftp_custom_cmd_param_struct *pCmdParam)
{
    oa_uint16 paramStrLen = oa_strlen(pStr);   
    oa_uint16 paramIndex = 0;
    oa_uint16 paramCharIndex = 0;
    oa_uint16 index = 0;

    while ((pStr[index] != 13 )   //carriage return
            && (index < paramStrLen) 
            && (paramIndex < MAX_NUM_OF_PARAM)) 
    {
        if(pStr[index] == ',' )
        { 
            /* Next param string */
            paramCharIndex = 0 ;
            paramIndex ++;
        } 
        else
        {
            if(pStr[index] != '"' )
            {
                if(paramCharIndex >= MAX_LEN_OF_PARAM)
                    return OA_FALSE;

                /*Get each of command param char, the param char except char ' " '*/
                pCmdParam->paramStr[paramIndex][paramCharIndex] = pStr[index];
                paramCharIndex ++;
            } 
        }     
        index ++;
    }

    pCmdParam->numOfParam = paramIndex+1;

    return OA_TRUE;
}



oa_bool ftp_start(oa_sockaddr_struct *pAddr, oa_char *pUser, oa_char *pPwd, oa_char *dlName, oa_uint32 ftp_flag)
{
    oa_bool ret = OA_TRUE;
    oa_char filename[32] = {0}, *pp = NULL;
    
    OA_DEBUG_L3("FTP:%s.", __func__);

    if(g_ftp_context.state  == FTP_STATE_NONE)
    {     
        ftp_reset_global_cntx(&g_ftp_context);

        oa_memcpy(g_ftp_context.ftp_username, pUser, oa_strlen(pUser));
        oa_memcpy(g_ftp_context.ftp_password, pPwd, oa_strlen(pPwd));

        oa_memcpy(&g_ftp_context.serv_ctrl_addr, pAddr, sizeof(oa_sockaddr_struct));          

        if(0 == oa_strlen(dlName))
            oa_memcpy(g_ftp_context.downloadFileName, (oa_char *)FTP_DLL_NAME, oa_strlen( (oa_char *)FTP_DLL_NAME));
        else
            oa_memcpy(g_ftp_context.downloadFileName, (oa_char *)dlName, oa_strlen(dlName));

        g_ftp_context.ftp_flag = ftp_flag;

        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_START);
        OA_DEBUG_L3("FTP:%s--STATE_NONE, send FTP_CMD_START.", __func__);
        ftp_send_cmd(FTP_CMD_START);
    }
    else
    {
        ret = OA_FALSE;
    }

    OA_DEBUG_L3("FTP:ftp_start: addrType=%d,addr_len=%d,addr=%s,port=%d,dlname=%s,flag=%d", 
                                                    g_ftp_context.serv_ctrl_addr_type,
                                                    g_ftp_context.serv_ctrl_addr.addr_len,
                                                    g_ftp_context.serv_ctrl_addr.addr,
                                                    g_ftp_context.serv_ctrl_addr.port,
                                                    g_ftp_context.downloadFileName,
                                                    ftp_flag);

    return ret;
}


void ftp_reset_global_cntx(ftp_context_struct *ftp_cntx)
{
    ftp_cntx->state = FTP_STATE_NONE;
    ftp_cntx->ctrl_sock_handle = -1;
    ftp_cntx->data_sock_handle = -1;
    
    ftp_cntx->downloadFileHandle = 0;
    ftp_cntx->downloadFileSize = 0;
    ftp_cntx->downloadDataCount = 0;
    ftp_cntx->cmdRetBuf = NULL;
    ftp_cntx->FatalErrorCount = 0;
    
    oa_memset(&ftp_cntx->serv_ctrl_addr, 0x00, sizeof(oa_sockaddr_struct));
    oa_memset(&ftp_cntx->serv_data_addr, 0x00, sizeof(oa_sockaddr_struct));
    oa_memset(ftp_cntx->ftp_username, 0x00, sizeof(ftp_cntx->ftp_username));
    oa_memset(ftp_cntx->ftp_password, 0x00, sizeof(ftp_cntx->ftp_password));
    oa_memset(ftp_cntx->ftp_cmd, 0x00, sizeof(ftp_cntx->ftp_cmd));
    oa_memset(ftp_cntx->downloadFileName, 0x00, 64);
    oa_memset(ftp_cntx->downloadFileNameFullpath, 0x00, 64);

    //oa_memcpy(ftp_cntx->downloadFileName, (oa_char *)FTP_DLL_NAME, oa_strlen( (oa_char *)FTP_DLL_NAME));
    oa_memcpy(ftp_cntx->downloadFileNameFullpath,  (oa_char *)FTP_DLL_BAK_PATH, oa_wstrlen(FTP_DLL_BAK_PATH)*2);

   	ftp_cntx->ftp_flag = 0x00;

    ftp_cntx->data_pipe_recon = 5;
}

oa_uint32 g_ftp_exception_code = 0x00;
void ftp_cmd_ret_parse(oa_char * pData, oa_uint32 dataLen)
{
    /*read command ret info from buffer and handle them*/
    oa_uint16 cmd;
    oa_char cmdStr[FTP_CMD_STR_LEN+1] = {0};
    oa_char *p;

    /*print server command info*/
    //UART_PutBytes(FTP_UART_PORT, (oa_uint8*)"<< ", oa_strlen("<< "), UART_GetOwnerID(FTP_UART_PORT));
    //UART_PutBytes(FTP_UART_PORT, (oa_uint8*)pData, oa_strlen(pData), UART_GetOwnerID(FTP_UART_PORT));
    OA_DEBUG_L3(">> %s",pData);
    
    if(dataLen==0 || pData == NULL)
        return;

    /*Find the first command, if multi-line returned, a '-' will found after return number*/
    p = oa_strstr(pData, "\r\n");

    /*maybe received several command once*/
    while(pData != NULL && (oa_strlen(pData) > (FTP_CMD_STR_LEN+1)))
    {
        if(FTP_IS_FIG(pData[0]) && FTP_IS_FIG(pData[1]) && FTP_IS_FIG(pData[2]) && pData[3]==' ')
        {
            /*good cmd-ret should be "XXX [string]."*/
            oa_memcpy(cmdStr, pData, FTP_CMD_STR_LEN);
            
            cmd = oa_atoi(cmdStr);

            switch(cmd)
            {
                case FTP_RET_CMD_FILESIZE:
                    g_ftp_context.downloadFileSize = oa_strtoul((oa_char *)(pData+FTP_CMD_STR_LEN+1), NULL, 10);
                break;

                case FTP_RET_CMD_PASVMOD:
                {
                    /*get service port for passive mode*/
                    oa_char *point = NULL, *end = NULL;
                    static ftp_custom_cmd_param_struct cmdParamData = {0};
                    
                    //OA_DEBUG_L3("I know it's FTP_RET_CMD_PASVMOD, just return!");

                    point = oa_strchr((oa_char*)pData, '(');
                    
                    oa_memset(&cmdParamData, 0x00, sizeof(ftp_custom_cmd_param_struct));
                    ftp_custom_command_parse_param((oa_char*)point+1, &cmdParamData);

                    //g_ftp_context.serv_data_addr.addr[0] = oa_atoi((oa_char*)cmdParamData.paramStr[0]);
                    //g_ftp_context.serv_data_addr.addr[1] = oa_atoi((oa_char*)cmdParamData.paramStr[1]);
                    //g_ftp_context.serv_data_addr.addr[2] = oa_atoi((oa_char*)cmdParamData.paramStr[2]);
                    //g_ftp_context.serv_data_addr.addr[3] = oa_atoi((oa_char*)cmdParamData.paramStr[3]);
                    oa_sprintf(g_ftp_context.serv_data_addr.addr, "%s.%s.%s.%s", 
                                                        cmdParamData.paramStr[0],
                                                        cmdParamData.paramStr[1],
                                                        cmdParamData.paramStr[2],
                                                        cmdParamData.paramStr[3]);
                    
                    g_ftp_context.serv_data_addr.port = oa_atoi((oa_char*)cmdParamData.paramStr[4])*256;
                    g_ftp_context.serv_data_addr.port += oa_atoi((oa_char*)cmdParamData.paramStr[5]);

                    g_ftp_context.serv_data_addr_type = OA_IP_ADDRESS;
                    g_ftp_context.serv_data_addr.addr_len = 4;
                    
                    OA_DEBUG_L3("FTP:%s--PASVMOD RAW:%s", __func__,point);
                    OA_DEBUG_L3("FTP:%s--GET PASV IP:%s", __func__, g_ftp_context.serv_data_addr.addr);
                    OA_DEBUG_L3("FTP:%s--GET PASV PORT:%s_%s--%d", __func__,
                                                    cmdParamData.paramStr[4],
                                                    cmdParamData.paramStr[5],
                                                    g_ftp_context.serv_data_addr.port);                    
                }
                break;

                case FTP_RET_CMD_BE_TICKED:
                case FTP_RET_CMD_USER_PW_INCORRECT:
                case FTP_RET_CMD_FILE_NOT_FIND:
                {
                    /*exception*/
                    g_ftp_exception_code = cmd;
                    cmd = FTP_CMD_FAIL;
                } 
                break;

                default:
                break;
            }
            
            OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, cmd);
            ftp_send_cmd(cmd);
        }

        /*point to next line of cmd-ret*/
        pData = p + 2;

        /*point to next command, if needed*/
        p = oa_strstr(pData, "\r\n");        
   }
    
}

void ftp_cmd_ret_timeout(void *param)
{
    /*Force to close ctrl and data socket*/
    OA_DEBUG_L3("FTP:ftp_cmd_ret_timeout");
    OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_TIMEROUT);
    ftp_send_cmd(FTP_CMD_TIMEROUT);
}

void ftp_data_transfer_timeout(void *param)
{
    OA_DEBUG_L3("FTP:ftp_data_transfer_timeout");
    OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_TIMEROUT);
    ftp_send_cmd(FTP_CMD_TIMEROUT);
}

void ftp_restart_module_timeout(void *param)
{
    oa_sleep(2000); 
    //restart module
    //we need to some other way to restart module
    oa_module_restart(NULL);
}

/************ FTP State machine for command handle************/
#define FTP_VB_BLK_LEN 2048
oa_uint8 ftp_crc_file_buffer[FTP_VB_BLK_LEN] = {0};
extern void ftp_misc_crc16(oa_uint8 aData[], oa_uint8 aDataout[], oa_uint32 aSize);
void ftp_state_cmd_handle(FTP_CMD_ENUM cmd)
{
    oa_int32 ret;
    oa_char ftp_info[50] = {0}; /*for print ftp information*/
	static oa_bool crc_suc = OA_FALSE;
	
    OA_DEBUG_L3("FTP:Beg ftp_state_cmd_handle:state=%s,cmd=%d", ftp_state_enum_name_map[g_ftp_context.state],cmd);
    
    oa_memset(g_ftp_context.ftp_cmd, 0, oa_strlen(g_ftp_context.ftp_cmd));
    
    if(FTP_CMD_FAIL == cmd || FTP_CMD_TIMEROUT == cmd)
    {
        /*exception handle*/
        g_ftp_context.state = FTP_STATE_NONE;

        if(g_ftp_context.downloadFileHandle>0)
        {
            oa_fclose(g_ftp_context.downloadFileHandle);
            g_ftp_context.downloadFileHandle=0;
        }

        if(g_ftp_context.data_sock_handle >= 0)
        {
            oa_soc_close(g_ftp_context.data_sock_handle);
        }

        if(g_ftp_context.ctrl_sock_handle >= 0)
        {
            oa_soc_close(g_ftp_context.ctrl_sock_handle);
        }

        /**/
        oa_timer_stop(FTP_CTRL_TIMER);
        oa_timer_stop(FTP_DATA_TIMER);

        if(FTP_CMD_TIMEROUT == cmd)
        {
            oa_sprintf(ftp_info, "+FTP:Timer out\r\n");
            if(ftp_rsp_cb_callback)
                (* ftp_rsp_cb_callback)(ftp_info);

        }
        else
        {
            oa_sprintf(ftp_info, "+FTP:Fail %d\r\n", g_ftp_exception_code);
            if(ftp_rsp_cb_callback)
                (* ftp_rsp_cb_callback)(ftp_info);
        }

        OA_DEBUG_L3(ftp_info);

        /*Castle add: restart the MD if needed after CRC check finished.*/
        if((g_ftp_context.state>= FTP_STATE_QUIT_LOGIN) && !(g_ftp_context.ftp_flag&FTP_FLAG_NO_RSET))
        {
            /*FTP upgrade finished need Reset target */
            ftp_restart_module_timeout(NULL);
        }
        return;
    }
    else
    {
        switch(g_ftp_context.state)
        {
            case FTP_STATE_NONE:
                if(FTP_CMD_START == cmd)
                {
                    ret = oa_soc_create(OA_SOCK_STREAM, 0);

                    if(ret >=0)
                    {
                        g_ftp_context.ctrl_sock_handle = ret;
                        g_ftp_context.state = FTP_STATE_CREATE_CTRL_SOCK;
                        OA_DEBUG_L3("FTP:%s--create socket for ctrl:%d", __func__, g_ftp_context.ctrl_sock_handle);
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_SUCCESS);
                        ftp_send_cmd(FTP_CMD_SUCCESS);
                    }
                    else
                    {
                        g_ftp_context.state = FTP_STATE_NONE;
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_FAIL);
                        OA_DEBUG_L3("create ctrl socket failed: %d", ret);
                        ftp_send_cmd(FTP_CMD_FAIL);
                    }
                }
            break;

            case FTP_STATE_CREATE_CTRL_SOCK:
                if(FTP_CMD_SUCCESS == cmd)
                {
                    OA_DEBUG_L3("Check before connect: %d,%d,%s:%d", 
                                    g_ftp_context.ctrl_sock_handle, 
                                    g_ftp_context.serv_ctrl_addr_type,
                                    g_ftp_context.serv_ctrl_addr.addr,
                                    g_ftp_context.serv_ctrl_addr.port);
                    ret = oa_soc_connect(g_ftp_context.ctrl_sock_handle, g_ftp_context.serv_ctrl_addr_type, &g_ftp_context.serv_ctrl_addr);
                    if(OA_SOC_SUCCESS == ret || OA_SOC_WOULDBLOCK == ret)
                    {
                        /*Create Success */
                        OA_DEBUG_L3("FTP:%s--connect wouldblock, waiting...%s", __func__, ret);
                        g_ftp_context.state = FTP_STATE_CONNECT_CTRL_PIPE;
                        if(OA_SOC_SUCCESS == ret)//can we directly go to next step?
                            ftp_send_cmd(FTP_CMD_SUCCESS);
                    }
                    else
                    {
                        /*Fail*/
                        g_ftp_context.state = FTP_STATE_NONE;
                        OA_DEBUG_L3("FTP:%s--connect failed.", __func__);
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_FAIL);
                        OA_DEBUG_L3("connect ctrl socket failed: %d", ret);
                        ftp_send_cmd(FTP_CMD_FAIL);
                    }            
                }
            break;

            case FTP_STATE_CONNECT_CTRL_PIPE:
                if(FTP_CMD_SUCCESS == cmd)
                {
                    oa_sprintf(ftp_info, "+FTP:Connected");
                }
                else if(FTP_RET_CMD_CONNECTED == cmd)
                {
                    oa_sprintf(g_ftp_context.ftp_cmd, "USER %s\r\n", g_ftp_context.ftp_username);
                    oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    g_ftp_context.state = FTP_STATE_LOGINON_USER;
                }
            break;

            case FTP_STATE_LOGINON_USER:
                if(FTP_RET_CMD_REQ_PW == cmd)
                {
                    oa_sprintf(g_ftp_context.ftp_cmd, "PASS %s\r\n", g_ftp_context.ftp_password);
                    oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    g_ftp_context.state = FTP_STATE_LOGINON_PASS;
                }           
            break;

            case FTP_STATE_LOGINON_PASS:
                if(FTP_RET_CMD_LOGGEDON == cmd)
                {
                    oa_sprintf(g_ftp_context.ftp_cmd, "TYPE I\r\n");
                    oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    g_ftp_context.state = FTP_STATE_TYPE;
                }
            break;

            case FTP_STATE_TYPE:
                if(FTP_RET_CMD_TYPE == cmd)
                {
                    oa_sprintf(g_ftp_context.ftp_cmd, "SIZE %s\r\n", g_ftp_context.downloadFileName);
                    oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    g_ftp_context.state = FTP_STATE_FILESIZE;
                }
            break;

            case FTP_STATE_FILESIZE:
                if(FTP_RET_CMD_FILESIZE == cmd)
                {
                    oa_sprintf(g_ftp_context.ftp_cmd, "PASV\r\n");
                    oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    g_ftp_context.state = FTP_STATE_PASV_MOD;

                    if(g_ftp_context.ftp_flag&FTP_FORCE_NEW_DLL)
                    {
                        /*need create new .bak file*/				
                        oa_fdelete((oa_uint16 *)g_ftp_context.downloadFileNameFullpath);
                    }
                    /*open or create file in FS*/
                    g_ftp_context.downloadFileHandle = oa_fopen((oa_uint16 *)g_ftp_context.downloadFileNameFullpath);
                    if(g_ftp_context.downloadFileHandle<0)
                    {
                        /*create downloadFile*/
                        g_ftp_context.downloadFileHandle = oa_fcreate((oa_uint16 *)g_ftp_context.downloadFileNameFullpath);
                        if(g_ftp_context.downloadFileHandle<0)
                        {
                            g_ftp_context.state = FTP_STATE_NONE;
                            OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_FAIL);
                            OA_DEBUG_L3("create file failed: %d", g_ftp_context.downloadFileHandle);
                            ftp_send_cmd(FTP_CMD_FAIL);
                        }
                    }
                    else
                    {/*if existed, continue to download :)*/
                        oa_fsize(g_ftp_context.downloadFileHandle, &g_ftp_context.downloadDataCount);
                        oa_fseek(g_ftp_context.downloadFileHandle, 0, OA_FILE_END);
                        if(g_ftp_context.downloadDataCount<g_ftp_context.downloadFileSize)
                        {
                            //oa_fseek(g_ftp_context.downloadFileHandle, g_ftp_context.downloadDataCount, OA_FILE_BEGIN);
                        }
                        else
                        {/*file is already downloaded*/
                            //g_ftp_context.FatalErrorCount++;
                        }
                    }
                }
            break;

            case FTP_STATE_PASV_MOD:
                if(FTP_RET_CMD_PASVMOD == cmd)
                {
                    /*create data socket*/
                    ret = oa_soc_create(OA_SOCK_STREAM, 0); 
                    if(ret >=0)
                    {
                        g_ftp_context.data_sock_handle = ret;
                        g_ftp_context.state = FTP_STATE_CREATE_DATA_SOCK;
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_SUCCESS);
                        ftp_send_cmd(FTP_CMD_SUCCESS);
                    }
                    else
                    {
                        /*Fail*/
                        g_ftp_context.state = FTP_STATE_NONE;
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_FAIL);
                        OA_DEBUG_L3("passive pipe create failed: %d", ret);
                        ftp_send_cmd(FTP_CMD_FAIL);
                    }
                }
            break;

            case FTP_STATE_CREATE_DATA_SOCK:
                if(FTP_CMD_SUCCESS == cmd)
                {                
                    ret = oa_soc_connect(g_ftp_context.data_sock_handle, g_ftp_context.serv_data_addr_type, &g_ftp_context.serv_data_addr);

                    if(OA_SOC_SUCCESS == ret || OA_SOC_WOULDBLOCK == ret)
                    {
                        /*Create Success */
                        g_ftp_context.state = FTP_STATE_CONNECT_DATA_PIPE;
                    }
                    else
                    {
                        /*fail*/
                        g_ftp_context.state = FTP_STATE_NONE;
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_FAIL);
                        OA_DEBUG_L3("passive pipe connect failed: %d", ret);
                        ftp_send_cmd(FTP_CMD_FAIL);
                    }
                }
            break;

            case FTP_STATE_CONNECT_DATA_PIPE:
                if(FTP_CMD_SUCCESS == cmd)
                {
                    /*set the download location first, 0: a new get*/
                    oa_sprintf(g_ftp_context.ftp_cmd, "REST %d\r\n", g_ftp_context.downloadDataCount);
                    oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    g_ftp_context.state = FTP_STATE_FILE_REST;
                }
            break;

            case FTP_STATE_FILE_REST:
                if(FTP_RET_CMD_REST == cmd)
                {
                    oa_sprintf(g_ftp_context.ftp_cmd, "RETR %s\r\n", g_ftp_context.downloadFileName);
                    oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    g_ftp_context.state = FTP_STATE_DLFILE_REQ;

                    /*sometime we received FTP_RET_CMD_DATA_PIPE_ACCEPT after data received.*/
                }
            break;

            case FTP_STATE_DLFILE_REQ:
                if(FTP_RET_CMD_DATA_PIPE_ACCEPT == cmd || FTP_RET_CMD_DATA_CONNECT_ALREADY == cmd)
                {
                    /*wait for the first block of data.*/
                    g_ftp_context.state = FTP_STATE_TRANSFER_GOING;                
                    oa_timer_start(FTP_DATA_TIMER, ftp_data_transfer_timeout, NULL, FTP_WAIT_DATA_DELAY);

                    //oa_sprintf(ftp_info, "+FTP:Downloading\r\n");
                }
            break;

            case FTP_STATE_TRANSFER_GOING:
                if((FTP_RET_CMD_TRANSFER_OK == cmd || FTP_RET_CMD_CLOSE_CONNECT == cmd)
                    && g_ftp_context.downloadDataCount >= g_ftp_context.downloadFileSize)
                {
                    oa_uint32 crcRead = 0;
                    oa_uint8 crcRrsult[8] = {0};
                    oa_int32 ret;
                    /*Check CRC first*/
                    //FS_Seek(g_ftp_context.downloadFileHandle, 0x00, OA_FILE_BEGIN);
                    //ASSERT(g_ftp_context.downloadDataCount<=(300*1024));
                    //FS_Read(g_ftp_context.downloadFileHandle, ftp_crc_file_buffer, g_ftp_context.downloadDataCount, &crcRead);
                    ret = oa_fsize(g_ftp_context.downloadFileHandle, &crcRead);
                    OA_DEBUG_L3("%s called: CRC read file OK! %d:%d", __func__, crcRead, ret);

                    if(!(g_ftp_context.ftp_flag&FTP_FLAG_NOT_CRC))
                    {
                        ftp_misc_crc16(ftp_crc_file_buffer, crcRrsult, crcRead);
                    }

                    oa_fclose(g_ftp_context.downloadFileHandle);

                    if(crcRrsult[0]==0x00 && crcRrsult[1]==0x00)
                    {
                        /*download finished, CRC check successfully, change file openat.dll.bak to openat.dll*/
                        if(g_ftp_context.ftp_flag&FTP_FLAG_NOT_DLL)
                        {
                            /*a normal file*/
                            static oa_wchar fn[128] = {0};
                            oa_char *pp;
                            //filter the folder name, get only file name
                            oa_memset(fn, 0x00, 128);
                            pp = g_ftp_context.downloadFileName + oa_strlen(g_ftp_context.downloadFileName) - 1;
                            while(pp > g_ftp_context.downloadFileName)
                            {
                                if(*pp == '/' || *pp == '\\')
                                {
                                    pp ++;
                                    break;
                                }
                                pp--;
                            }
                            //OA_DEBUG_L3("GET FILENAME------------------------>%s", pp);
                            oa_ascii2unicode((oa_int8 *)fn, pp);
                            oa_fdelete((const oa_uint16 *)fn);//delete the outdated file if existed
                            oa_frename((const oa_uint16 *)FTP_DLL_BAK_PATH, (const oa_uint16 *)fn);
                            crc_suc = OA_TRUE;//dummy true, no need of crc check
                            OA_DEBUG_L3("%s called: normal save ok", __func__);
                        }
                        else
                        {
                            /*a dll file*/
                            oa_fdelete((const oa_uint16 *)FTP_DLL_PATH);
                            oa_frename((const oa_uint16 *)FTP_DLL_BAK_PATH, (const oa_uint16 *)FTP_DLL_PATH);
                            crc_suc = OA_TRUE;
                            OA_DEBUG_L3("%s called: CRC success!", __func__);
                        }
                    }
                    else
                    {
                        /*CRC check failed! delete the backup openat.dll.bak*/
                        oa_fdelete((const oa_uint16 *)FTP_DLL_BAK_PATH);
                        crc_suc = OA_FALSE;
                        OA_DEBUG_L3("%s called: CRC failed!", __func__);
                    }

                    g_ftp_context.downloadFileHandle=0;
                    
                    oa_timer_stop(FTP_DATA_TIMER);
                    if(g_ftp_context.ctrl_sock_handle >= 0)
                    {
                        oa_sprintf(g_ftp_context.ftp_cmd, "QUIT\r\n");
                        oa_soc_send(g_ftp_context.ctrl_sock_handle,(oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd));
                    }
                    else
                    {
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_RET_CMD_GOODBYE);
                        ftp_send_cmd(FTP_RET_CMD_GOODBYE);
                    }
                    g_ftp_context.state = FTP_STATE_QUIT_LOGIN;

                    //oa_sprintf(ftp_info, "FTP:Finished size=%d\r\n",g_ftp_context.downloadDataCount);
                }
                else if(FTP_RET_CMD_CLOSE_CONNECT == cmd)
                {
                    OA_DEBUG_L3("Data abort before finished. Return with failed.");
                    ftp_send_cmd(FTP_CMD_FAIL);
                }
                else if(FTP_RET_CMD_TRANSFER_OK == cmd)
                {
                    /*this is the most common situation
                    * FTP_RET_CMD_TRANSFER_OK will always comes before data transferring finished.
                    * always ignore it, and will send it again manually when finished transferring.
                    */
                }

            break;

            case FTP_STATE_QUIT_LOGIN:
                if(FTP_RET_CMD_GOODBYE == cmd)
                {
                    g_ftp_context.state = FTP_STATE_NONE;

                    if(g_ftp_context.data_sock_handle >= 0)
                    {
                        oa_soc_close(g_ftp_context.data_sock_handle);
                    }

                    if(g_ftp_context.ctrl_sock_handle >= 0)
                    {
                        oa_soc_close(g_ftp_context.ctrl_sock_handle);
                    }
                    g_ftp_context.data_sock_handle= -1;
                    g_ftp_context.ctrl_sock_handle= -1;                    
                    /**/
                    oa_timer_stop(FTP_CTRL_TIMER);

                    if(g_ftp_context.downloadDataCount<g_ftp_context.downloadFileSize
                        && g_ftp_context.FatalErrorCount<5)
                    {
                        /*For continue start ftp download*/
                        g_ftp_context.FatalErrorCount++;
                        OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_START);
                        ftp_send_cmd(FTP_CMD_START);

                        oa_sprintf(ftp_info, "+FTP:Restart");
                    }
                    else
                    {
                        oa_sprintf(ftp_info, "FTP:Finished size=%d. Waiting Restart!",g_ftp_context.downloadDataCount);
                        /*Restart target*/
                        //UART_PutBytes(FTP_UART_PORT, (oa_uint8*)ftp_info, oa_strlen(ftp_info), UART_GetOwnerID(FTP_UART_PORT));
                        OA_DEBUG_L3(ftp_info);

                        if(!crc_suc)
                        {
                            if(ftp_rsp_cb_callback)
                                (* ftp_rsp_cb_callback)("+FTP:CRC Failed\r\n");
                        }
                        else
                        {
                            if(g_ftp_context.downloadDataCount >= g_ftp_context.downloadFileSize)
                            {
                                if(ftp_rsp_cb_callback)
                                    (* ftp_rsp_cb_callback)("+FTP:Finished\r\n");
                            }
                            else
                            {                        
                                if(ftp_rsp_cb_callback)
                                    (* ftp_rsp_cb_callback)("+FTP:Data Error\r\n");
                            }
                        }

                        if(!(g_ftp_context.ftp_flag&FTP_FLAG_NO_RSET))
                        {
                            /*FTP upgrade finished need Reset target */
                            oa_timer_start(FTP_CTRL_TIMER, ftp_restart_module_timeout, NULL, 2000);

                            if(ftp_rsp_cb_callback)
                                (* ftp_rsp_cb_callback)("+FTP:Restart\r\n");
                        }
                        //really finished ftp downloading
                        return;
                    }
                }
            break;

            default:
                break;
        }

        if(g_ftp_context.state != FTP_STATE_NONE)
        {
            /*if not in FTP_STATE_NONE, restart Ctrl timer*/
            oa_timer_start(FTP_CTRL_TIMER, ftp_cmd_ret_timeout, NULL, FTP_WAIT_CMD_RET_DELAY);
        }

    }   

    if(oa_strlen(ftp_info))
    {
        /*print ftp information if exist*/
        OA_DEBUG_L3(ftp_info);
    }

    if(oa_strlen(g_ftp_context.ftp_cmd))
    {
        /*print send server command  if exist*/
        //UART_PutBytes(FTP_UART_PORT, (oa_uint8*)">> ", oa_strlen(">> "), UART_GetOwnerID(FTP_UART_PORT));
        //UART_PutBytes(FTP_UART_PORT, (oa_uint8*)g_ftp_context.ftp_cmd, oa_strlen(g_ftp_context.ftp_cmd), UART_GetOwnerID(FTP_UART_PORT));
        OA_DEBUG_L3("<< %s", g_ftp_context.ftp_cmd);
    }

    OA_DEBUG_L3("FTP:End ftp_state_cmd_handle:state=%s", ftp_state_enum_name_map[g_ftp_context.state]);
    
    return;
}

/*Control socket notify*/
void ftp_ctrl_socket_notify_ind(oa_app_soc_notify_ind_struct *pMsg)
{
    oa_int32 ret = 0;
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    oa_app_soc_notify_ind_struct *soc_notify = pMsg;

    switch(soc_notify->event_type)
    {
        case OA_SOC_WRITE:
        {
            if (soc_notify->result == OA_TRUE)
            {
            }
            else
            {
            }     
        }
        break;

        case OA_SOC_READ:
        {
            if (soc_notify->result == OA_TRUE)
            {
                oa_memset(g_ftp_rec_databuf, 0 , (MAX_FTP_NETWORK_BUFLEN*sizeof(oa_uint8)));

                ret = oa_soc_recv(soc_notify->socket_id , (oa_uint8*)g_ftp_rec_databuf, MAX_FTP_NETWORK_BUFLEN, 0);

                if(ret >= 0)
                {
                    //parse ftp sever string cmd
                    ftp_cmd_ret_parse((oa_char *)g_ftp_rec_databuf, ret);                   
                }
                else if(OA_SOC_WOULDBLOCK == ret)
                {
                    /* Waiting for MSG_ID_APP_SOC_NOTIFY_IND */
                }
                else
                {
                    //oa_sprintf(printBuffer, "READ ERROR\r\n");  
                }            
            }
            else
            {
                //oa_sprintf(printBuffer, "RECV ERROR:%d\r\n",soc_notify->error_cause);  
            }     
        }           
        break;

        case OA_SOC_ACCEPT:
        break;

        case OA_SOC_CONNECT:
        {
            if (soc_notify->result == OA_TRUE)
            {
                OA_DEBUG_L3("%s called: connect ok, next step.", __func__);
                ftp_send_cmd(FTP_CMD_SUCCESS);
            }
            else
            {
                OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_FAIL);
                OA_DEBUG_L3("WouldBlock return failed: %d", soc_notify->result);
                ftp_send_cmd(FTP_CMD_FAIL);
            }             
        }       
        break;

        case OA_SOC_CLOSE:
        {
            if (soc_notify->result == OA_TRUE)
            {
            }
            else
            {
                if(OA_SOC_CONNRESET == soc_notify->error_cause)
                {
                    /*If the socket is closed by remote server, use OA_SOC_CLOSE to close local socketID*/
                    oa_soc_close(soc_notify->socket_id);
                    /**/
                }
            }

            g_ftp_context.ctrl_sock_handle = -1;
        }
        break;

        default:
        break;
    }
}

/*data socket notify when downloading data*/
void ftp_data_socket_notify_ind(oa_app_soc_notify_ind_struct *pMsg)
{
    oa_int32 ret = 0;
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    oa_app_soc_notify_ind_struct *soc_notify = pMsg;

    switch(soc_notify->event_type)
    {
        case OA_SOC_WRITE:
        {
            if (soc_notify->result == OA_TRUE)
            {
            }
            else
            {
            }     
        }
        break;

        case OA_SOC_READ:
        {
            if (soc_notify->result == OA_TRUE)
            {
                oa_uint32 Written;
                oa_double dl_rate;
                
                do{
                    oa_memset(g_ftp_rec_databuf, 0 , (MAX_FTP_NETWORK_BUFLEN*sizeof(oa_uint8)));
                    ret = oa_soc_recv(soc_notify->socket_id , (oa_uint8*)g_ftp_rec_databuf, MAX_FTP_NETWORK_BUFLEN, 0);

                    if(ret > 0)
                    {
                        dl_rate = ret/(((oa_get_time_stamp() - g_ftp_context.last_time_stamp)*4.615)/1000);
                        g_ftp_context.last_time_stamp = oa_get_time_stamp();
                        
                        OA_DEBUG_L3("Get %d:%d bytes from FTP server, with %.1fB/s.", ret, g_ftp_context.downloadDataCount, dl_rate);
                        oa_fwrite(g_ftp_context.downloadFileHandle, g_ftp_rec_databuf, ret, &Written);
                        OA_DEBUG_L3("Actually witten to file: %d", Written);

                        if(ret!= Written)
                        {
                            OA_DEBUG_USER("File cannot write anymore!!! FAILED!!!");
                            ftp_send_cmd(FTP_CMD_FAIL);
                        }
                        
                        g_ftp_context.downloadDataCount += Written;

                        if(g_ftp_context.ftp_flag&FTP_FLAG_EN_PCT)
                        {
                            oa_char pct_buf[64]={0};
                            oa_sprintf(pct_buf, "+FTP:PCT=%d%%\r\n", g_ftp_context.downloadDataCount*100/g_ftp_context.downloadFileSize);
                            if(ftp_rsp_cb_callback)
                                (* ftp_rsp_cb_callback)(pct_buf);
                        }

                        OA_DEBUG_L3("FTP:RECV DATA->%d of %d finished!", g_ftp_context.downloadDataCount, g_ftp_context.downloadFileSize);

                        if((g_ftp_context.downloadDataCount >= g_ftp_context.downloadFileSize )&&(g_ftp_context.downloadFileSize!=0))
                        {
                            OA_DEBUG_L3("FTP:RECV DATA->total finished!");
                            OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_RET_CMD_TRANSFER_OK);
                            /*send FTP_RET_CMD_TRANSFER_OK manually, cause the server send it before and ignored by client*/
                            ftp_send_cmd(FTP_RET_CMD_TRANSFER_OK);
                        }
                        else if((g_ftp_context.downloadDataCount<g_ftp_context.downloadFileSize)&&(g_ftp_context.downloadFileSize!=0))
                        {
                            //kal_trace_on_file("FTP:downloadCount bigger than downloadFileSize!! %d", g_ftp_context.downloadDataCount);

                            //transfer going restart timer
                            if(g_ftp_context.state == FTP_STATE_TRANSFER_GOING)                
                            {
                                oa_timer_start(FTP_DATA_TIMER, ftp_data_transfer_timeout, NULL, FTP_WAIT_DATA_DELAY);
                                oa_timer_start(FTP_CTRL_TIMER, ftp_cmd_ret_timeout, NULL, FTP_WAIT_CMD_RET_DELAY);
                            }
                        }
                        return;
                    }
                    else if(OA_SOC_WOULDBLOCK == ret)
                    {
                        /* Waiting for MSG_ID_APP_SOC_NOTIFY_IND */
                        return;
                    }
                    else
                    {
                        //oa_sprintf(printBuffer, "READ ERROR\r\n");  
                    }   
                }while(ret > 0);
            }
            else
            {
                //oa_sprintf(printBuffer, "RECV ERROR:%d\r\n",soc_notify->error_cause);  
            }

        }           
        break;

        case OA_SOC_ACCEPT:
        break;

        case OA_SOC_CONNECT:
        {
            if (soc_notify->result == OA_TRUE)
            {
                /*data socket connect success, need send event*/
                OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_SUCCESS);
                ftp_send_cmd(FTP_CMD_SUCCESS);
            }
            else
            {
                OA_DEBUG_L3("%s called: ftp_send_cmd launched cmd = %d", __func__, FTP_CMD_FAIL);
                OA_DEBUG_L3("WouldBlock return failed: %d", soc_notify->result);
                ftp_send_cmd(FTP_CMD_FAIL);
            }             
        }       
        break;

        case OA_SOC_CLOSE:
        {
            if (soc_notify->result == OA_TRUE)
            {
            }
            else
            {
                if(OA_SOC_CONNRESET == soc_notify->error_cause)
                {
                    /*If the socket is closed by remote server, use OA_SOC_CLOSE to close local socketID*/
                    oa_soc_close(soc_notify->socket_id);

                    OA_DEBUG_USER("FTP DATA PIPE was closed by server, make another try? %d:%d",
                                            g_ftp_context.downloadDataCount,
                                            g_ftp_context.downloadFileSize);
                    if(g_ftp_context.state == FTP_STATE_TRANSFER_GOING
                        &&((g_ftp_context.downloadDataCount<g_ftp_context.downloadFileSize)
                        &&(g_ftp_context.downloadFileSize!=0)))
                    {
                        oa_fclose(g_ftp_context.downloadFileHandle);
                        
                        if(--g_ftp_context.data_pipe_recon > 0)
                        {                        
                            //goto another process to get data.
                            g_ftp_context.state = FTP_STATE_TYPE;
                            ftp_send_cmd(FTP_RET_CMD_TYPE);
                        }
                        else
                        {
                            //waiting for timeout?
                        }
                    }
                }
            }

            g_ftp_context.data_sock_handle = -1;
        }
        break;

        default:
        break;
    }
}

/*for FTP at command and SMS get the socket connect parameters*/
oa_bool tcpip_get_ftp_param(ftp_custom_cmd_param_struct *pParamData, oa_sockaddr_struct *pSockAddr, oa_socket_addr_enum *addr_type)
{
    oa_bool bIpValidity = OA_FALSE;
    oa_uint8 dumyaddr[OA_MAX_SOCK_ADDR_LEN] = {0};

    /* Parse the socket address */
    if(oa_soc_ip_check((oa_char*)pParamData->paramStr[0], dumyaddr, &bIpValidity))
    {
        if(bIpValidity)
        {
            /*Is the IP address*/
            *addr_type = OA_IP_ADDRESS;
        }
        else
        {
            /*Is the Domain Name*/
            *addr_type = OA_DOMAIN_NAME;
        }
    }
    else
    {
        /*Is the Domain Name*/   
        *addr_type = OA_DOMAIN_NAME;
    }
    pSockAddr->addr_len = 4;
    pSockAddr->sock_type = OA_SOCK_STREAM;

    oa_strcpy(pSockAddr->addr, pParamData->paramStr[0]);
    
    /*
    if(pParamData->paramStr[4][0] == '0' || pParamData->paramStr[4][0] == '1')
    {
        pParamData->paramStr[4][0] = (pParamData->paramStr[4][0] - '0');
    }
    else if(pParamData->paramStr[4][0] != 0)
    {
        //invalid parameter
        return OA_FALSE;
    }
    */
    return OA_TRUE;
}

void ftp_rsp_register(oa_ftp_rsp_cb_fptr cb)
{
    ftp_rsp_cb_callback = cb;
}


/*SMS content parse whether need start ftp download*/
oa_bool ftp_core_step_handler(oa_uint8 * data)
{
    oa_char buffer[100] = {0};
    oa_bool ret = OA_FALSE;
    static ftp_custom_cmd_param_struct cmdParamData = {0};
    oa_sockaddr_struct sockAddr = {0};
    oa_char headStr[5] = {0};
    oa_ftp_state_enum ftp_state = OA_FTP_FAIL;

    OA_DEBUG_L3("FTP:ftp_core_step_handler:data=%s",data);

    oa_memcpy(headStr, (oa_char *)data, 4);
    /*Convert a string to all-lower-case string first*/
    oa_strtolower(headStr);

    OA_DEBUG_L3("FTP:ftp_core_step_handler:1");
    
    /*check if valid ftp string*/
    if(!oa_memcmp(headStr, "ftp:", oa_strlen("ftp:")))
    {
        oa_memset(&cmdParamData, 0x00, sizeof(ftp_custom_cmd_param_struct));
        ftp_custom_command_parse_param((oa_char*)(data+4), &cmdParamData);

        if(tcpip_get_ftp_param(&cmdParamData, &sockAddr, &g_ftp_context.serv_ctrl_addr_type))
        {
            /* SMS Command format OK */
            oa_uint32 flag = oa_strtoul(cmdParamData.paramStr[5], NULL, 10);
            //OA_DEBUG_L3("FTP:ftp_core_step_handler:2 SMS Command format OK");

            sockAddr.port = oa_strtoul(cmdParamData.paramStr[1], NULL, 10);

            ret = ftp_start(&sockAddr, cmdParamData.paramStr[2], cmdParamData.paramStr[3], cmdParamData.paramStr[4], flag);   

            if(ret == 1)
            {
                oa_sprintf(buffer, "+FTP:Start %d\r\n",flag);
                ftp_state = OA_FTP_START;
                ret = OA_TRUE;
            }
            else
            {
                oa_sprintf(buffer, "+FTP:Not Start\r\n",ret);
                ftp_state = OA_FTP_NOT_START;
            }
        }
        else
        {
            oa_sprintf(buffer, "+FTP:Param Error\r\n");  
            ftp_state = OA_FTP_PARAM_ERR;
        }
        
        if(ftp_rsp_cb_callback)
            (* ftp_rsp_cb_callback)(buffer);
    }

    return ret;    
}

//FileHandle shuold be opened
oa_uint8 ftp_get_byte_from_virtual_buf(oa_fs_handle FileHandle, oa_uint32 FileSize)
{
	static oa_int32 block_index = 0, block_max;
	static oa_int32 in_block_ptr = 0;
	static oa_uint32 cur_block_len = 0;

	if(in_block_ptr == cur_block_len)
	{
		/*well, we finished one block, read next*/
		block_max = (FileSize/FTP_VB_BLK_LEN) + (FileSize%FTP_VB_BLK_LEN ? 1 : 0);
		oa_fseek(FileHandle, block_index*FTP_VB_BLK_LEN, OA_FILE_BEGIN);
		oa_fread(FileHandle, ftp_crc_file_buffer, FTP_VB_BLK_LEN, &cur_block_len);
		in_block_ptr = 0;
		if(++block_index >= block_max)
			block_index = 0;
	}

	return ftp_crc_file_buffer[in_block_ptr++];
}

/*run the initialization when the GSM network is ready*/
void ftp_init(void)
{
    OA_DEBUG_L3("%s called.", __func__);
    ftp_reset_global_cntx(&g_ftp_context);
}

void ftp_misc_crc16(oa_uint8 aData[], oa_uint8 aDataout[], oa_uint32 aSize) 
{
    oa_uint8 CRC16Lo, CRC16Hi;   // 8-bit unsigned entity.
    oa_uint8 CL, CH, size;   // 8-bit unsigned entity.
    oa_uint8 SaveHi, SaveLo;   // 8-bit unsigned entity.
    oa_int32 I, Flag;

    //  size=sizeof(aData)/sizeof(unsigned oa_char);
    CRC16Lo=0x00;
    CRC16Hi=0x00;

    CL = 0x01;
    CH = 0xa0;

    for (I = 0 ;I<=aSize-1;I++ )
    {
		oa_uint8 tData = ftp_get_byte_from_virtual_buf(g_ftp_context.downloadFileHandle, aSize);
		CRC16Lo = CRC16Lo ^ tData ;      //
        //CRC16Lo = CRC16Lo ^ aData[I] ;      //

        for (Flag = 0 ;Flag<= 7;Flag++ )
        {
            SaveHi = CRC16Hi;
            SaveLo = CRC16Lo;

            CRC16Hi = CRC16Hi >> 1;           // '高位右移一位
            CRC16Lo = CRC16Lo >> 1;           // '低位右移一位

            if ((SaveHi & 0x01) == 0x01)      //'如果高位字节最后一位为1
            {
                CRC16Lo = CRC16Lo + 0x80;          //'则低位字节右移后前面补1
            }
            else
            {
                //'否则自动补0
            }
            if ((SaveLo & 0x01)  == 0x01)    //'如果LSB为1，则与多项式码进行异或
            {
                CRC16Hi = CRC16Hi ^ CH;
                CRC16Lo = CRC16Lo ^ CL;
            }
        }
    }

    aDataout[1]= CRC16Hi;   // 'CRC高位      
    aDataout[0]= CRC16Lo;   // ' 'CRC低位
}

