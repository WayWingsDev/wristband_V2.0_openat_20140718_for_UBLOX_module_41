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
 *   oa_gprs.h
 *
 * Project:
 * --------
 *   OPEN AT project
 *
 * Description:
 * ------------
 *   This Module gives a sample app on open at framework.
 *
 * Author:
 * -------
 *   None!
 *
 ****************************************************************************/
#ifndef _OA_SOC_H_
#define _OA_SOC_H_


#define OA_GPRS_BUFFER_SIZE 20480
#define OA_APP_GPRS_SENDING_SIZE 1024

typedef enum 
{
    OA_SOC_STATE_OFFLINE,
    OA_SOC_STATE_CONNECT,
    OA_SOC_STATE_ONLINE,
    OA_SOC_STATE_HALT
}oa_soc_state;

typedef enum 
{
    OA_COMM_NULL = 0x00,
    OA_COMM_WORKMODE,
    OA_COMM_SENDFREQ,
    OA_COMM_ALARMSW,
    OA_COMM_IP,
    OA_COMM_BANDPHONE,//5
    OA_COMM_RESTART,
    OA_COMM_CHECKSTATUS,
    OA_COMM_CALLTHEROLL,
    OA_COMM_UPGRADE,
    OA_COMM_CALLBACK = 0x10,
    OA_COMM_CHECKTIME = 0x11,
    OA_COMM_GENERALRETURN = 0x12,
    OA_COMM_LOGINRETURN = 0x13,
    OA_COMM_LOCATIONRETURN = 0x14,
    OA_COMM_VERSION = 0x15,
    OA_COMM_HEARTBEATFREQ = 0x16
}oa_comm_protocol_enum;

typedef struct
{
    oa_soc_state state;                             //socket connect current state
    oa_buffer *gprs_rx;
    oa_buffer *gprs_tx;
    oa_uint8 gprs_tx_pending_data[OA_APP_GPRS_SENDING_SIZE];
    oa_uint16 gprs_tx_pending_size;
    oa_uint8 gprs_rx_pending_data[OA_APP_GPRS_SENDING_SIZE];
    oa_uint16 gprs_rx_pending_size;
    
    oa_int8 socket_id;                                //socket connect id
    oa_uint32 recon_counter;
    oa_bool can_connect;
    oa_bool is_blocksend;      //socket is blocksend or not

    oa_soc_apn_info_struct apn_info;             //gprs APN information
    oa_sockaddr_struct soc_addr;                //socket connect server address
    oa_socket_addr_enum addr_type;  //socket connect server address type:IP Addror Domain Name

}oa_soc_context;
extern oa_soc_context g_soc_context;

extern void oa_soc_init_fast(void);
extern void oa_soc_init(void);
extern oa_uint8 oa_soc_connect_req(void);
extern void oa_soc_close_req(void);
extern void oa_soc_send_req(void);
extern void oa_soc_gprs_recv(oa_uint8* data, oa_uint16 len);
extern void oa_soc_notify_ind(void *inMsg);
extern void oa_soc_start_heartbeat_timer(void);
extern void oa_soc_send_heartbeat_data(void* param);

oa_buffer *oa_create_ring_buffer_noinit(oa_uint8 *rawBuf, oa_uint32 maxLength);
void oa_free_buffer_noinit(oa_buffer *buf);
oa_uint32 oa_write_buffer_noinit(oa_buffer *buf, oa_uint8 *data, oa_uint32 len);
oa_uint32 oa_query_buffer_noinit(oa_buffer *buf, oa_uint8 *data, oa_uint32 len);
oa_uint32 oa_read_buffer_noinit(oa_buffer *buf, oa_uint8 *data, oa_uint32 len);
oa_uint32 oa_write_buffer_force_noinit(oa_buffer *buf, oa_uint8 *data, oa_uint32 len);


#endif/*_OA_SOC_H_*/

