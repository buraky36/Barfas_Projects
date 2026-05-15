/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    system.h
 * @author  Tuya Team
 * @version v1.0.7
 * @date    2020.11.9
 * @brief   UART data processing.The user does not need to care about what the file implements.
 */


#ifndef __SYSTEM_H_
#define __SYSTEM_H_

#include "protocol.h" 
#ifdef SYSTEM_GLOBAL
  #define SYSTEM_EXTERN
#else
  #define SYSTEM_EXTERN   extern
#endif

//=============================================================================
//Byte order of the frame
//=============================================================================
#define         HEAD_FIRST                      0
#define         HEAD_SECOND                     1        
#define         PROTOCOL_VERSION                2
#define         FRAME_TYPE                      3
#define         LENGTH_HIGH                     4
#define         LENGTH_LOW                      5
#define         DATA_START                      6

//=============================================================================
//Data frame type
//=============================================================================
#define         PRODUCT_INFO_CMD                1                               //Product information
#define         WIFI_STATE_CMD                  2                               //Wifi working status
#define         WIFI_RESET_CMD                  3                               //Reset wifi
#define         WIFI_MODE_CMD                   4                               //Select smartconfig/AP mode
#ifdef STATE_FAST_UPLOAD_ENABLE
#define         STATE_UPLOAD_CMD                0x18                            //Status quick upload
#define         STATE_RC_UPLOAD_CMD             0x19                            //Report status quick upload
#else
#define         STATE_UPLOAD_CMD                0x05                            //Status upload
#define         STATE_RC_UPLOAD_CMD             0x08                            //Report status report
#endif
#define         GET_LOCAL_TIME_CMD              0x06                            //Get local time
#define         WIFI_TEST_CMD                   0x07                            //Wifi function test
#define         DATA_QUERT_CMD                  0x09                            //Order send
#define         WIFI_UG_REQ_CMD                 0x0a                            //Request WIFI module firmware upgrade
#define         ROUTE_RSSI_CMD                  0x0b                            //Query the signal strength of the currently connected router

#define         MCU_UG_REQ_CMD                  0x0c                            //Request MCU firmware upgrade
#define         UPDATE_START_CMD                0x0d                            //MCU upgrade start
#define         UPDATE_TRANS_CMD                0x0e                            //MCU upgrade transfer
#define         GET_GL_TIME_CMD                 0x10                            //Get green time
#define         GET_DP_CACHE_CMD                0x15                            //Get dp cache instruction
#define         REPORTED_MCU_SN_CMD             0x17                            //Report to SN of MCU
#ifdef LOCK_KEEP_ALIVE
#define         GET_WIFI_STATE_CMD              0x1A                            //Get WiFi status
#endif
#define         WIFI_RESET_NOTICE_CMD           0x25                            //Module reset status notification



//=============================================================================
#define MCU_RX_VER              0x00                                            //Module send frame protocol version number
#define MCU_TX_VER              0x00                                            //MCU send frame protocol version number(default)
#define PROTOCOL_HEAD           0x07                                            //Fixed protocol header length
#define FRAME_FIRST             0x55                                            //Frame header first byte
#define FRAME_SECOND            0xaa                                            //Frame header second byte

#define FIRM_UPDATA_SIZE        256                                             //Upgrade package size

//============================================================================= 

#define Tx_Error_Timeout 10


SYSTEM_EXTERN unsigned char wifi_data_process_buf[PROTOCOL_HEAD + WIFI_DATA_PROCESS_LMT];       //Serial data processing buffer
SYSTEM_EXTERN unsigned char volatile wifi_uart_rx_buf[PROTOCOL_HEAD + WIFI_UART_RECV_BUF_LMT];  //Serial receive buffer
SYSTEM_EXTERN unsigned char wifi_uart_tx_buf[PROTOCOL_HEAD + WIFIR_UART_SEND_BUF_LMT];          //Serial port send buffer
//
SYSTEM_EXTERN volatile unsigned char *queue_in;
SYSTEM_EXTERN volatile unsigned char *queue_out;

SYSTEM_EXTERN unsigned char stop_update_flag;                                                 //ENABLE:Stop all data uploads   DISABLE:Restore all data uploads

#ifndef WIFI_CONTROL_SELF_MODE
SYSTEM_EXTERN unsigned char reset_wifi_flag;                                                  //Reset wifi flag (TRUE: successful / FALSE: failed)
SYSTEM_EXTERN unsigned char set_wifimode_flag;                                                //Set the WIFI working mode flag (TRUE: Success / FALSE: Failed)
SYSTEM_EXTERN unsigned char wifi_work_state;                                                  //Wifi module current working status
#endif


/**
 * @brief  Write wifi uart bytes
 * @param[in] {dest} UART send buffer starts writing the address
 * @param[in] {byte} Write byte value
 * @return UART send buffer ends write address
 */
unsigned short set_wifi_uart_byte(unsigned short dest, unsigned char byte);

/**
 * @brief  Write wifi uart buffer
 * @param[in] {dest} UART send buffer starts writing the address
 * @param[in] {src} source address
 * @param[in] {len} Data length
 * @return UART send buffer ends write address
 */
unsigned short set_wifi_uart_buffer(unsigned short dest, unsigned char *src, unsigned short len);

/**
 * @brief  Calculate checksum
 * @param[in] {pack} Data source pointer
 * @param[in] {pack_len} Need to calculate the length of the checksum data
 * @return checksum
 */
unsigned char get_check_sum(unsigned char *pack, unsigned short pack_len);

/**
 * @brief  Send a frame of data to the wifi serial port
 * @param[in] {fr_type} Frame type
 * @param[in] {fr_ver} Frame version
 * @param[in] {len} Data length
 * @return Null
 */
void wifi_uart_write_frame(unsigned char fr_type, unsigned short len);

/**
 * @brief  Get the serial number of the specified DPID in the array
 * @param[in] {dpid} dpid
 * @param[out] {p_dp_type} dp type
 * @return SUCCESS/ERROR
 */
unsigned char get_dp_type(unsigned char dpid, unsigned char *p_dp_type);

/**
 * @brief  Data frame processing
 * @param[in] {offset} Data start position
 * @return Null
 */
void data_handle(unsigned short offset);

/**
 * @brief  Determines whether there is data in the uart receive buffer
 * @param  Null
 * @return Is there data
 */
unsigned char get_queue_total_data(void);

/**
 * @brief  Read uart receive buffer 1 byte data
 * @param  Null
 * @return Read the data
 */
unsigned char Queue_Read_Byte(void);

#endif
  
  
