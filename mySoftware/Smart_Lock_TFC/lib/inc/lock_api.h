/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    lock_api.h
 * @author  Tuya Team
 * @version v1.0.7
 * @date    2020.11.9
 * @brief   The door lock function interface is called directly where it is needed to process the logic in the returned data.
 */
 
 
#ifndef __LOCK_API_H_
#define __LOCK_API_H_


#ifdef LOCK_API_GLOBAL
  #define LOCK_API_EXTERN
#else
  #define LOCK_API_EXTERN   extern
#endif

#include "wifi.h"

#include "stm32g0xx.h"

#define DECODE_MAX_LEN                          16                              //Maximum length after offline dynamic password decryption
  
  
//=============================================================================
//Lock data frame type
//=============================================================================
#define         TEMP_PASS_CMD                   0x11                            //Request cloud temporary password (only supports single group)
#define         PASS_CHECK_CMD                  0x12                            //Dynamic password verification
#define         MUL_TEMP_PASS_CMD               0x13                            //Request cloud temporary password (support multiple groups)
#define         SCHEDULE_TEMP_PASS_CMD          0x14                            //Request cloud temporary password (with schedule list)
#define         OFFLINE_DYN_PW_CMD              0x16                            //Offline dynamic password
#define         PICTURE_EVENT_STATE_CMD         0x60                            //Event status notification
#define         PICTURE_UPLOAD_CMD              0x61                            //upload picture
#define         PICTURE_UPLOAD_RETURN_CMD       0x62                            //Image upload result feedback
#define         PICTURE_UPLOAD_STATE_GET_CMD    0x63                            //Get image upload status
#ifdef PHOTO_LOCK_PICTURE_UPLOAD_ENABLE
#define         PHOTO_LOCK_PICTURE_UPLOAD_CMD   0x64                            //Photo door lock picture upload related functions
#endif

//=============================================================================
//Subcommand
//=============================================================================
#ifdef PHOTO_LOCK_PICTURE_UPLOAD_CMD
#define         EVENT_TRIGGER_CAPTURE_NOTICE_SUBCMD         0x00                //Event triggers snapshot notification
#endif



#ifdef LOCK_API_ENABLE
/**
 * @brief  MCU requests to obtain temporary cloud password
 * @param  Null
 * @return Null
 * @note   After the MCU actively calls, the temporary password and validity period can be obtained in the temp_pass_handle function
 */
void mcu_get_temp_pass(void);

/**
 * @brief  MCU request temporary password return function
 * @param[in] {suc_flag} Request flag (1:success  0:failure)
 * @param[in] {gl_time} Password validity period Green time (6 digits from low to high, respectively year, month, day, hour, minute, second)
 * @param[in] {pass} Temporary password data (ascll code indicates)
 * @param[in] {pass_len} Temporary password data length
 * @return Null
 * @note   After the MCU actively calls mcu_get_temp_pass, the temporary password and validity period in this function
 */
void temp_pass_handle(unsigned char suc_flag, const unsigned char gl_time[], 
                      const unsigned char pass[], unsigned char pass_len);


/**
 * @brief  MCU requests dynamic password verification
 * @param[in] {time} Request the current green time (from low to high 6 digits, respectively year, month, day, hour, minute, second)
 * @param[in] {user_pass} The dynamic password entered by the user (from low to high 8 digits, the password is passed in with ASCLL, range '0'-'9')
 * @param[in] {admin_num} Number of administrator password groups (0 ~ 10, if it is 0, the following parameters are invalid)
 * @param[in] {admin_len} Administrator password length (up to 8 bytes)
 * @param[in] {admin_pass} Multiple sets of administrator passwords (multiple sets of passwords are arranged in sequence, length admin_num * admin_len, passed in with ASCLL, range '0'-'9')
 * @return Null
 * @note   The MCU actively calls, after the call is successful, the dynamic password verification result can be obtained in the pass_check_handle function
 */
void dynamic_pass_check(unsigned char time[], unsigned char user_pass[], unsigned char admin_num,
                        unsigned char admin_len, unsigned char admin_pass[]);

/**
 * @brief  The MCU requests to verify the dynamic password return function
 * @param[in] {status} Check mark (0: successful; 1: failure; 2: not activated; 3: length error)
 * @return Null
 * @note   After the MCU initiatively calls dynamic_pass_check successfully, the check result can be obtained in this function
 */
void pass_check_handle(unsigned char status);

/**
 * @brief  The MCU requests multiple sets of temporary passwords in the cloud
 * @param  Null
 * @return Null
 * @note   After the active call of the MCU is completed, the temporary password parameters and the validity period can be obtained in the mul_temp_pass_data function
 */
void mcu_get_mul_temp_pass(void);

/**
 * @brief  MCU requests multiple sets of temporary password return function
 * @param[in] {data} Return data
 * @return Null
 * @note   Null
 */
void mul_temp_pass_handle(const unsigned char data[]);

/**
 * @brief  MCU requests to obtain temporary cloud password (with schedule list)
 * @param  Null
 * @return Null
 * @note   After the active call of the MCU is completed, the result can be obtained in the mul_temp_pass_data function
 */
void mcu_get_schedule_temp_pass(void);

/**
 * @brief  MCU requests a temporary password (with schedule list) to return function
 * @param[in] {data} Return data
 * @return Null
 * @note   Null
 */
void schedule_temp_pass_handle(const unsigned char data[]);

#ifdef OFFLINE_DYN_PW_ENABLE
/**
 * @brief  Offline dynamic password
 * @param[in] {green_time} green time
 *              green_time[0] is the year and 0x00 is the year 2000.
 *              green_time[1] is the month, starting from 1 to ending at 12
 *              green_time[2] is the date, starting from 1 to 31
 *              green_time[3] is the clock, starting from 0 to ending at 23
 *              green_time[4] is minutes, starting from 0 to ending at 59
 *              green_time[5] is seconds, starting from 0 to ending at 59
 * @param[in] {pw} Offline dynamic password
 * @param[in] {pw_len} Offline dynamic password length
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the offline_dynamic_password_result function
 */
void offline_dynamic_password(unsigned char green_time[],unsigned char pw[],unsigned char pw_len);

/**
 * @brief  Offline dynamic password results
 * @param[in] {result_data} result data
 * @return Null
 * @note   The MCU needs to call the offline_dynamic_password function first, and then process the received result in this function
 */
void offline_dynamic_password_result(unsigned char result_data[]);
#endif

#ifdef PICTURE_UPLOAD_ENABLE
/**
 * @brief  Image upload event status notification
 * @param[in] {event} Event information coding
 * @param[in] {picture} Whether to carry picture information
 * @param[in] {pic_num} Uploaded pictures (<=10)
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the picture_event_state_notice_result function
 */
void picture_event_state_notice(unsigned short event, unsigned char picture, unsigned char pic_num);

/**
 * @brief  Image upload event status notification result
 * @param[in] {result} result
 * @return Null
 * @note   The MCU needs to call the picture_event_state_notice function first, and then process the received result in this function
 */
void picture_event_state_notice_result(unsigned char result);

/**
 * @brief  upload picture
 * @param[in] {p_time} Time data
 * @param[in] {picture_id} Picture id number
 * @param[in] {pic_total_num} Picture total package
 * @param[in] {pic_num} Current packet number (starting from 0)
 * @param[in] {p_pic_data} Picture data
 * @param[in] {pic_data_len} Picture data length
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the picture_upload_result function
 */
void picture_upload(unsigned char p_time[], unsigned short picture_id, unsigned short pic_total_num, unsigned short pic_num, \
                                            unsigned char p_pic_data[], unsigned char pic_data_len);

/**
 * @brief  Image upload result
 * @param[in] {result} result
 * @return Null
 * @note   The MCU needs to call the picture_upload function first, and then process the received result in this function
 */
void picture_upload_result(unsigned char result);

/**
 * @brief  Image upload result feedback
 * @param[in] {p_value} result data
 * @param[in] {data_len} result data length
 * @return Null
 * @note   The MCU needs to process the received results by itself.
 */
void picture_upload_return(unsigned char p_value[], unsigned short data_len);

/**
 * @brief  Get image upload status
 * @param  Null
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the picture_upload_state_get_result function
 */
void picture_upload_state_get(void);

/**
 * @brief  Image upload status get result
 * @param[in] {p_value} Result data
 * @param[in] {data_len} Result data length
 * @return Null
 * @note   The MCU needs to call the picture_upload_state_get function first, and then process the received result in this function
 */
void picture_upload_state_get_result(unsigned char p_value[], unsigned short data_len);
#endif

#ifdef LOCK_KEEP_ALIVE
/**
 * @brief  Keep alive IO wake
 * @param[in] Null
 * @return Null
 * @note   MCU needs to implement logic code by itself
 */
void keep_alive_awake(void);

/**
 * @brief  Get WIFI status
 * @param  Null
 * @return Null
 * @note   After the MCU needs to call by itself, the result can be processed in the get_wifi_state_result function
 */
void mcu_get_wifi_state(void);

/**
 * @brief  mcu receives WiFi status
 * @param[in] {p_data} Data
 * @return Null
 * @note   MCU needs to call the mcu_get_wifi_state function by itself, and then process the received result in this function
 */
void get_wifi_state_result(const unsigned char p_data[]);
#endif

#ifdef PHOTO_LOCK_PICTURE_UPLOAD_ENABLE
/**
 * @brief  MCU sending event triggers snapshot notification
 * @param[in] {event_infor_num} Event information encoding
 * @param[in] {capture_flag} Whether to take a picture
 * @param[in] {capture_type} Snapshot type
 * @param[in] {capture_num} Number of captured pictures(<=6)
 * @param[in] {time_flag} Time flag  0:Without time  1:Local time  2:Green time
 * @param[in] {p_time} Time data
 * @return Null
 * @note   After the MCU needs to call by itself, the result can be processed in the event_trigger_capture_notice_result function.
 */
void mcu_event_trigger_capture_notice(unsigned short event_infor_num, unsigned char capture_flag, unsigned char capture_type,
                                        unsigned char capture_num, unsigned char time_flag, unsigned char p_time[]);
                                            
/**
 * @brief  Photo door lock picture upload related functions
 * @param[in] {p_data} Data
 * @return Null
 * @note   
 */
void photo_lock_picture_upload_func(const unsigned char p_data[]);
#endif

#endif

#endif
