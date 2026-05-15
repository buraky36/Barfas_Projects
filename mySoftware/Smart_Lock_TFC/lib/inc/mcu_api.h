/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    mcu_api.h
 * @author  Tuya Team
 * @version v1.0.7
 * @date    2020.11.9
 * @brief   The function under this file does not need to be modified by the user. 
            The files that the user needs to call are in the file.
 */


#ifndef __MCU_API_H_
#define __MCU_API_H_

#include "wifi.h"

#include "stm32g0xx.h"
//#include "stm32g0xx_hal.h"
//#include "stm32g0xx_hal_def.h"
#ifdef MCU_API_GLOBAL
  #define MCU_API_EXTERN
#else
  #define MCU_API_EXTERN   extern
#endif

/**
 * @brief  hex to bcd
 * @param[in] {Value_H} High Byte
 * @param[in] {Value_L} Low Byte
 * @return Converted data
 */
unsigned char hex_to_bcd(unsigned char Value_H,unsigned char Value_L);

/**
 * @brief  Calculate string length
 * @param[in] {str} String address
 * @return data length
 */
unsigned long my_strlen(unsigned char *str);

/**
 * @brief  Set the first count bytes of the memory area pointed to by src to the character c
 * @param[out] {src} source address
 * @param[in] {ch} Set character
 * @param[in] {count} Set the data length
 * @return Source address after data processing
 */
void *my_memset(void *src,unsigned char ch,unsigned short count);

/**
 * @brief  Memory copy
 * @param[out] {dest} target address
 * @param[in] {src} source address
 * @param[in] {count} number of data copies
 * @return Source address after data processing
 */
void *my_memcpy(void *dest, const void *src, unsigned short count);

/**
 * @brief  String copy
 * @param[in] {dest} target address
 * @param[in] {src} source address
 * @return Source address after data processing
 */
char *my_strcpy(char *dest, const char *src);

/**
 * @brief  String compare
 * @param[in] {s1} String 1
 * @param[in] {s2} String 2
 * @return Size comparison value
 * -             0:s1=s2
 * -             <0:s1<s2
 * -             >0:s1>s2
 */
int my_strcmp(char *s1 , char *s2);

/**
 * @brief  Split the int type into four bytes
 * @param[in] {number} 4 bytes of original data
 * @param[out] {value} 4 bytes of data after processing is completed
 * @return Null
 */
void int_to_byte(unsigned long number,unsigned char value[4]);

/**
 * @brief  Combine 4 bytes into 1 32bit variable
 * @param[in] {value} 4 bytes of data after processing is completed
 * @return 32bit variable after the merge is completed
 */
unsigned long byte_to_int(const unsigned char value[4]);


/**
 * @brief  raw type dp data upload
 * @param[in] {dpid} id number
 * @param[in] {value} Current dp value pointer
 * @param[in] {len} data length
 * @return Null
 * @note   Null
 */
unsigned char mcu_dp_raw_update(unsigned char dpid,const unsigned char value[],unsigned short len);

/**
 * @brief  bool type dp data upload
 * @param[in] {dpid} id number
 * @param[in] {value} Current dp value pointer
 * @return Null
 * @note   Null
 */
unsigned char mcu_dp_bool_update(unsigned char dpid,unsigned char value);

/**
 * @brief  value type dp data upload
 * @param[in] {dpid} id number
 * @param[in] {value} Current dp value pointer
 * @return Null
 * @note   Null
 */
unsigned char mcu_dp_value_update(unsigned char dpid,unsigned long value);

/**
 * @brief  string type dp data upload
 * @param[in] {dpid} id number
 * @param[in] {value} Current dp value pointer
 * @param[in] {len} data length
 * @return Null
 * @note   Null
 */
unsigned char mcu_dp_string_update(unsigned char dpid,const unsigned char value[],unsigned short len);

/**
 * @brief  enum type dp data upload
 * @param[in] {dpid} id number
 * @param[in] {value} Current dp value pointer
 * @return Null
 * @note   Null
 */
unsigned char mcu_dp_enum_update(unsigned char dpid,unsigned char value);

/**
 * @brief  fault type dp data upload
 * @param[in] {dpid} id number
 * @param[in] {value} Current dp value pointer
 * @return Null
 * @note   Null
 */
unsigned char mcu_dp_fault_update(unsigned char dpid,unsigned long value);

/**
 * @brief  mcu gets bool type to send dp value
 * @param[in] {value} dp data buffer address
 * @param[in] {len} data length
 * @return current dp value
 * @note   Null
 */
unsigned char mcu_get_dp_download_bool(const unsigned char value[],unsigned short len);

/**
 * @brief  mcu gets enum type to send dp value
 * @param[in] {value} dp data buffer address
 * @param[in] {len} data length
 * @return current dp value
 * @note   Null
 */
unsigned char mcu_get_dp_download_enum(const unsigned char value[],unsigned short len);

/**
 * @brief  mcu gets value type to send dp value
 * @param[in] {value} dp data buffer address
 * @param[in] {len} data length
 * @return current dp value
 * @note   Null
 */
unsigned long mcu_get_dp_download_value(const unsigned char value[],unsigned short len);

/**
 * @brief  Receive data processing
 * @param[in] {value} UART receives byte data
 * @return Null
 * @note   Call this function in the MCU serial receive function and pass the received data as a parameter.
 */
void uart_receive_input(unsigned char value);

/**
 * @brief  Wifi serial port processing service
 * @param  Null
 * @return Null
 * @note   Call this function in the MCU main function while loop
 */
void wifi_uart_service(void);

/**
 * @brief  Protocol serial port initialization function
 * @param  Null
 * @return Null
 * @note   This function must be called in the MCU initialization code
 */
void wifi_protocol_init(void);


/**
 * @brief  Reported data combination report
 * @param[in] {time} Time data length 7, the first byte indicates whether to transmit the flag bit, and the rest are year, month, day, hour, minute, and second
 * @param[in] {dp_node} dp data structure. You need to create an array of this structure type before calling this function, the array length is dp number,
                               Cache the record dp data in this structure, the dp value needs to be stored in the corresponding type of structure member according to the dp data type.
 * @param[in] {dp_node_num} dp number
 * @return Result
 * -           0(ERROR): Fault
 * -           1(SUCCESS): Success
 * @note   This function must be called in the MCU initialization code
 */
unsigned char dp_record_combine_update(unsigned char time[], t_DP_NODE dp_node[], unsigned char dp_node_num);


#ifndef WIFI_CONTROL_SELF_MODE
/**
 * @brief  MCU gets reset wifi success flag
 * @param  Null
 * @return Reset flag
 * -           0(RESET_WIFI_ERROR):failure
 * -           1(RESET_WIFI_SUCCESS):success
 * @note   1:The MCU actively calls mcu_reset_wifi() and calls this function to get the reset state.
 *         2:If the module is in self-processing mode, the MCU does not need to call this function.
 */
unsigned char mcu_get_reset_wifi_flag(void);

/**
 * @brief  MCU actively resets wifi working mode
 * @param  Null
 * @return Null
 * @note   1:The MCU actively calls to obtain whether the reset wifi is successful through the mcu_get_reset_wifi_flag() function.
 *         2:If the module is in self-processing mode, the MCU does not need to call this function.
 */
void mcu_reset_wifi(void);

/**
 * @brief  Get set wifi status success flag
 * @param  Null
 * @return wifimode flag
 * -           0(SET_WIFICONFIG_ERROR):failure
 * -           1(SET_WIFICONFIG_SUCCESS):success
 * @note   1:The MCU actively calls to obtain whether the reset wifi is successful through the mcu_get_reset_wifi_flag() function.
 *         2:If the module is in self-processing mode, the MCU does not need to call this function.
 */
unsigned char mcu_get_wifimode_flag(void);

/**
 * @brief  MCU set wifi working mode
 * @param[in] {mode} enter mode
 * @ref        0(SMART_CONFIG):enter smartconfig mode
 * @ref        1(AP_CONFIG):enter AP mode
 * @return Null
 * @note   1:MCU active call
 *         2:After success, it can be judged whether set_wifi_config_state is TRUE; TRUE means successful setting of wifi working mode
 *         3:If the module is in self-processing mode, the MCU does not need to call this function.
 */
void mcu_set_wifi_mode(unsigned char mode);

/**
 * @brief  The MCU actively obtains the current wifi working status.
 * @param  Null
 * @return wifi work state
 * -          SMART_CONFIG_STATE:smartconfig config status
 * -          AP_STATE:AP config status
 * -          WIFI_NOT_CONNECTED:WIFI configuration succeeded but not connected to the router
 * -          WIFI_CONNECTED:WIFI configuration is successful and connected to the router
 * -          WIFI_CONN_CLOUD:WIFI is connected to the cloud server
 * -          WIFI_LOW_POWER:WIFI is in low power mode
 * -          SMART_AND_AP_STATE: WIFI smartconfig&AP mode
 * @note   1:If the module is in self-processing mode, the MCU does not need to call this function.
 */
unsigned char mcu_get_wifi_work_state(void);
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU gets system time for proofreading local clock
 * @param  Null
 * @return Null
 * @note   The MCU is called actively, and the data returned by the module can be processed in the mcu_write_rtctime function
 */
void mcu_get_system_time(void);
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  MCU obtains Green Time, used to proofread the local clock
 * @param  Null
 * @return Null
 * @note   After the active call of the MCU is completed, 
 *         the Green Time is recorded and calculated in the mcu_write_gltime function, 
 *         which is used for the timestamp verification of the door lock class
 */
void mcu_get_gelin_time(void);
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  Mcu initiated wifi function test
 * @param  Null
 * @return Null
 * @note   The MCU needs to call this function by itself.
 */
void mcu_start_wifitest(void);
#endif

/**
 * @brief  MCU requests wifi firmware upgrade
 * @param  Null
 * @return Null
 * @note   The MCU is called actively, and the data returned by the module can be processed in the wifi_update_handle function
 */
void wifi_update_request(void);

#ifdef DP_CACHE_SUPPORT
/**
 * @brief  Gets the dp cache instruction
 * @param[in] {table} Dp table, which stores the dp that the user needs to query. If the user needs to query all dp, the table can be directly filled with NULL
 * @param[in] {dp_num} The number of dp's that need to be queried, and if all dp's need to be queried, dp_num will just fill in 0
 * @return Null
 * @note   The MCU is called actively
 */
void get_dp_cache(unsigned char *table,unsigned char dp_num);
#endif

#ifdef REPORTED_MCU_SN_ENABLE
/**
 * @brief  MCU reported SN
 * @param[in] {sn} SN number
 * @param[in] {sn_len} SN number length
 * @return Null
 * @note   The MCU is called actively, and the data returned by the module can be processed in the mcu_sn_updata_result function
 */
void mcu_sn_updata(unsigned char sn[],unsigned char sn_len);
#endif

#endif
