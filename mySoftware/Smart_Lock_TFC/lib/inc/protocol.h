/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.h
 * @author  Tuya Team
 * @version v1.0.7
 * @date    2020.11.9
 * @brief                
 *                       *******Very important, be sure to watch!!!********
 *          1. The user implements the data delivery/reporting function in this file.
 *          2. DP ID / TYPE and data processing functions require the user to implement according to the actual definition
 *          3. There are #err hints inside the function that needs the user to implement the code after starting some macro definitions. 
 *             Please delete the #err after completing the function.
 */


#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#include <stdio.h>
#include "stm32g0xx.h"
//#include "stm32g0xx_hal.h"
//#include "stm32g0xx_hal_def.h"

/******************************************************************************
                 User related information configuration
******************************************************************************/

/******************************************************************************
              Configure whether lock firmware development is supported                  
To support MCU lock class development, open the macro
The MCU can call the lock_api.c file to implement the lock class specific interface.

                        ********WARNING!!!**********
The door lock class development supports firmware upgrades by default, with a firmware upgrade package of 256 bytes.
If this feature is turned on, the serial port receive buffer will become larger.
******************************************************************************/
#define         LOCK_API_ENABLE                         //Open the lock API interface

#ifdef LOCK_API_ENABLE
//#define         SUPPORT_MCU_FIRM_UPDATE                 //Enable MCU firmware upgrade (lock class development, supported by default)
#endif

/******************************************************************************
                            1:Modify product information              
******************************************************************************/
#define PRODUCT_KEY "5a2qdc2deoi2sopp"    //The unique product identification generated after product is created on development platform

#define MCU_VER "1.0.0"                   //User's software version for MCU firmware upgrade, MCU upgrade version needs to be modified

#define CONFIG_MODE CONFIG_MODE_SPECIAL

#define CONFIG_MODE_DELAY_TIME 10


/*  Special configuration for smart mode and AP mode.
    If you do not use this define, you switch between smart mode and AP mode    */
#define CONFIG_MODE_CHOOSE        0         //Open both AP and smart distribution networks without user switching, and the corresponding distribution network state 0x06
//#define CONFIG_MODE_CHOOSE        1         //Use only AP network configuration mode

/*  Equipment capacity selection  */
//#define PICTURE_UPLOAD_ENABLE       //Does the device need to support the image upload function?  0: not supported, 1: supported
//#define CAP_COMMU_MODE_ENABLE       //Which communication method does the device use to upload pictures?  0: indicates serial port, 1: indicates SPI
#define WIFI_RESET_NOTICE_ENABLE    //Does the device support module reset status notification?  0: not supported, 1: supported

/*  Keep Alive Options  */
//#define LOCK_KEEP_ALIVE        //If it is a keep-alive door lock, please open this macro

/******************************************************************************
                         2:Define the send and receive buffer:
                    If the current use of MCU RAM is not enough, can be appropriately reduced
******************************************************************************/
#ifndef SUPPORT_MCU_FIRM_UPDATE
#define WIFI_UART_RECV_BUF_LMT          700             //default 16 - UART data receiving buffer size, can be reduced if the MCU has insufficient RAM
#define WIFI_DATA_PROCESS_LMT           500            //default 24 - UART data processing buffer size, according to the user DP data size, must be greater than 24
#else
#define WIFI_UART_RECV_BUF_LMT          256             //default 128 - UART data receiving buffer size, can be reduced if the MCU has insufficient RAM
#define WIFI_DATA_PROCESS_LMT           600             //default 300 - UART data processing buffer size. If the MCU firmware upgrade is required, the single-packet size is 256, the buffer must be greater than 260
#endif

#define WIFIR_UART_SEND_BUF_LMT         96              //According to the user's DP data size, it must be greater than 48

/******************************************************************************
                      3:Does the MCU need to support the time function?
Open this macro if needed and implement the code in mcu_write_rtctime in the Protocol.c file.
Mcu_write_rtctime has #err hint inside, please delete the #err after completing the function
Mcu can call the mcu_get_system_time() function to initiate the calibration function after the wifi module is properly networked.
******************************************************************************/
#define         SUPPORT_MCU_RTC_CHECK                //Turn on time calibration

/******************************************************************************
                      13:Whether the MCU need to support the acquisition of green time function
Open the macro if necessary and implement the mcu_get_greentime code in protocol.c
There is a #err hint inside mcu_get_greentime. Please delete the #err after completing the function.
The MCU can call the mcu_get_green_time() function to initiate the calibration function after the wifi module is properly networked.
******************************************************************************/
//#define         SUPPORT_GREEN_TIME                //Turn on green time
#ifdef LOCK_API_ENABLE
#ifndef SUPPORT_GREEN_TIME
#define         SUPPORT_GREEN_TIME                //Turn on green time
#endif
#endif

/******************************************************************************
                      4:Does the MCU need to support the wifi function test?                    
Please enable this macro if necessary, and mcu calls mcu_start_wifitest in mcu_api.c file when wifi function test is required.
And view the test results in the protocol_c file wifi_test_result function.
There is a #err hint inside wifi_test_result. Please delete the #err after completing the function.
******************************************************************************/
#define         WIFI_TEST_ENABLE                //Open WIFI production test function (scan designated route)

/******************************************************************************
                          5:Does MCU support get dp cache instruction
For some sensors with configuration or control functions, the dp delivery function needs to be added. 
When the device is offline, a command is issued on the panel. This command will be cached in the cloud, waiting for the device to obtain it. 
The cache command is incremental, and the command that has been acquired will not be issued when it is acquired again.

If you need to support the dp cache instruction, please turn on this macro
******************************************************************************/
//#define         DP_CACHE_SUPPORT                   //Get dp cache instruction

/******************************************************************************
                          6:Does MCU support offline dynamic password
When the door lock is offline, the user can create a custom temporary password through the app. 
At this time, the module can verify the correctness of the password even when it is offline. 
The effective time control depends on Green Time
                          
If you need to support the offline dynamic password function, please turn on this macro, and you also need to turn on the macro to get Green Time
******************************************************************************/
#define         OFFLINE_DYN_PW_ENABLE              //Support offline dynamic password

/******************************************************************************
                          7:Whether the MCU supports reporting the SN number of the MCU
Can report the MCU SN number to the platform
If you need to support the reporting of the SN number of the MCU, please open this macro
******************************************************************************/
//#define         REPORTED_MCU_SN_ENABLE             //Support to report SN of MCU

/******************************************************************************
                          8:Whether the MCU supports fast status reporting
If you need to quickly report the support status, please open the macro
******************************************************************************/
//#define         STATE_FAST_UPLOAD_ENABLE            //Support status quick report

/******************************************************************************
                          9:Whether MCU supports photo lock image upload function
If you need to support the function of uploading photos and door locks, please enable this macro.
******************************************************************************/
//#define     PHOTO_LOCK_PICTURE_UPLOAD_ENABLE    //Support picture upload related functions of photo door lock



/******************************************************************************
                        1:dp data point serial number redefinition
          **This is the automatic generation of code, 
            such as the relevant changes in the development platform, 
            please re-download MCU_SDK**  
******************************************************************************/
//Unlock with Fingerprint(Only report)
#define DPID_UNLOCK_FINGERPRINT 1
//Unlock with Code(Only report)
#define DPID_UNLOCK_PASSWORD 2
//Unlock with Temporary Code(Only report)
#define DPID_UNLOCK_TEMPORARY 3
//Unlock with Card(Only report)
#define DPID_UNLOCK_CARD 5
//Unlock with Mechanical Key(Only report)
#define DPID_UNLOCK_KEY 7
//Alerts(Only report)
#define DPID_ALARM_LOCK 8
//Remote Unlocking Countdown(Only report)
#define DPID_UNLOCK_REQUEST 9
//Battery Level(Only report)
#define DPID_BATTERY_STATE 11
//Remaining Battery(Only report)
#define DPID_RESIDUAL_ELECTRICITY 12
//Double Lock(Only report)
#define DPID_REVERSE_LOCK 13
//Remote Unlocking in App (Only report)
#define DPID_UNLOCK_APP 15
//Unlock from Inside(Only report)
#define DPID_OPEN_INSIDE 17
//Door Status(Only report)
#define DPID_CLOSED_OPENED 18
//SMS Notification(Issue and report)
#define DPID_MESSAGE 20
//Synchronize All Fingerprints(Only report)
#define DPID_UPDATE_ALL_FINGER 25
//Synchronize All Codes(Only report)
#define DPID_UPDATE_ALL_PASSWORD 26
//Synchronize All Cards(Only report)
#define DPID_UPDATE_ALL_CARD 27
//Offline Code Unlocking(Only report)
#define DPID_UNLOCK_OFFLINE_PD 32
//Clearing offline codes(Only report)
#define DPID_UNLOCK_OFFLINE_CLEAR 33
//Query Added Method(Only report)
#define DPID_QUERY_CREATE_TYPE 34
//Added Method (Issue and report)
#define DPID_CREATE_TYPE_ACK 35
//Added (Only report)
#define DPID_DOOR_INPUT_OK 36
//Added (Issue and report)
#define DPID_CREATE_OK 37
//Remotely Delete Fingerprint(Issue and report)
#define DPID_REMOTE_DELETE_FP 39
//Remotely Delete Card (Issue and report)
#define DPID_REMOTE_DELETE_CD 43
//Set Key for No-Code Unlock(Issue and report)
#define DPID_REMOTE_NO_PD_SETKEY 49
//No-Code Unlock(Issue and report)
#define DPID_REMOTE_NO_DP_KEY 50
//Locking Status(Issue and report)
#define DPID_LOCK_MOTOR_STATE 55
//Lock Records(Issue and report)
#define DPID_LOCK_RECORD 57
//Associate Local Lock Capability(Issue and report)
#define DPID_LOCAL_CAPACITY_LINK 60
//Added(Issue and report)
#define DPID_DOOR_INPUT_OK_KIT 62
//Door opening direction setting(Issue and report)
#define DPID_LOCK_MOTOR_DIRECTION 65
//Auto Lock(Issue and report)
#define DPID_AUTOMATIC_LOCK 68
//Auto Lock Delay(Issue and report)
#define DPID_AUTO_LOCK_TIME 69
//Door lock local operation record(Issue and report)
#define DPID_LOCK_LOCAL_RECORD 70
//Temporary Password Modify(Issue and report)
#define DPID_TEMPORARY_PASSWORD_MODIFY 101
//Temporary Password Delete(Issue and report)
#define DPID_TEMPORARY_PASSWORD_DELETE 102
//Temporary Password Creat(Issue and report)
#define DPID_TEMPORARY_PASSWORD_CREAT 103
//Link Mode(Issue and report)
#define DPID_LINK_MODE 104
//Unlock Voice Remote(Issue and report)
#define DPID_UNLOCK_VOICE_REMOTE 105
//Unlock Phone Remote(Issue and report)
#define DPID_UNLOCK_PHONE_REMOTE 106
//Synch Method(Issue and report)
#define DPID_SYNCH_METHOD 107
//Unlock Method Modify(Issue and report)
#define DPID_UNLOCK_METHOD_MODIFY 108
//Unlock Method Delete(Issue and report)
#define DPID_UNLOCK_METHOD_DELETE 109
//Unlock Method Create(Issue and report)
#define DPID_UNLOCK_METHOD_CREATE 110
//Active message push(Issue and report)
#define DPID_INITIATIVE_MESSAGE 212
//Admin Key(Issue and report)
#define DPID_UNLOCK_ADMIN_KIT 61
//Arm Mode(Issue and report)
#define DPID_ARMING_SWITCH 10
//Forced Double Lock(Issue and report)
#define DPID_ENFORCE_LOCK_UP 48
//Combined Unlocking Records(Issue and report)
#define DPID_UNLOCK_DOUBLE_KIT 44
//Cancel Remote Adding (Issue and report)
#define DPID_CANCLE_REMOTE_CREATE 42
//Remotely Delete Code (Issue and report)
#define DPID_REMOTE_DELETE_PSW 41
//Remotely Delete Response(Only report)
#define DPID_REMOTE_DELETE_ACK 40
//Remote Deletion Allowed (Only report)
#define DPID_LOCK_ALLOW_DELETE 38
//Doorbell(Only report)
#define DPID_DOORBELL 19
//Child Lock(Only report)
#define DPID_CHILD_LOCK 14
//Battery Percentage(Issue and report)
#define DPID_BATTERY_PERCENTAGE 111

#define TUYA_WRONG_FINGERPRINT 0
#define TUYA_WRONG_PASSWORD 1
#define TUYA_WRONG_CARD 2

/**
 * @brief  Send data processing
 * @param[in] {value} Serial port receives byte data
 * @return Null
 * @note   Please fill the MCU serial port sending function into this function 
 *            and pass the received data into the serial port sending function as parameters
 */
void uart_transmit_output(unsigned char value);

/**
 * @brief  All dp point information of the system is uploaded to realize APP and muc data synchronization
 * @param  Null
 * @return Null
 * @note   This function SDK needs to be called internally;
 *         The MCU must implement the data upload function in the function;
 *         including only reporting and reportable hair style data.
 */
void all_data_update(void);

/**
 * @brief  dp delivery processing function
 * @param[in] {dpid} DP number
 * @param[in] {value} dp data buffer address
 * @param[in] {length} dp data length
 * @return Dp processing results
 * -           0(ERROR): failure
 * -           1(SUCCESS): success
 * @note   The function user cannot modify
 */
unsigned char dp_download_handle(unsigned char dpid,const unsigned char value[], unsigned short length);

/**
 * @brief  Get the sum of all dp commands
 * @param[in] Null
 * @return Sent the sum of the commands
 * @note   The function user cannot modify
 */
unsigned char get_download_cmd_total(void);


#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU proofreads local RTC clock
 * @param[in] {time} Get the time data
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void mcu_write_rtctime(unsigned char time[]);
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  Gets the green time
 * @param[in] {time} Get the time data
 * @return Null
 * @note   After the MCU actively calls mcu_get_gelin_time, the Green Time can be obtained in this function
 */
void mcu_write_gltime(unsigned char gl_time[]);
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  Wifi function test feedback
 * @param[in] {result} Wifi function test
 * @ref       0: failure
 * @ref       1: success
 * @param[in] {rssi} Test success indicates wifi signal strength / test failure indicates error type
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void wifi_test_result(unsigned char result,unsigned char rssi);
#endif

/**
 * @brief  MCU requests wifi module firmware upgrade to return results
 * @param[in] {status} Check mark
 * @return Null
 * @note   After the MCU calls the wifi_update_request function, it can get the module to return the result
 */
void wifi_update_handle(unsigned char status);

#ifdef SUPPORT_MCU_FIRM_UPDATE
/**
 * @brief  MCU requests MCU firmware upgrade
 * @param  Null
 * @return Null
 * @note   After the active call of the MCU is completed, the current status of the upgrade can be obtained in the mcu_update_handle function
 */
void mcu_update_request(void);

/**
 * @brief  MCU request mcu firmware upgrade return function
 * @param[in] {status} Check mark
 * @return Null
 * @note   After the MCU actively calls the mcu_update_request function, the current status of the upgrade can be obtained in this function
 */
void mcu_update_handle(unsigned char status);

/**
 * @brief  MCU enters firmware upgrade mode
 * @param[in] {value} Firmware buffer
 * @param[in] {position} The current data packet is in the firmware location
 * @param[in] {length} Current firmware package length (when the firmware package length is 0, it indicates that the firmware package is sent)
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
unsigned char mcu_firm_update_handle(const unsigned char value[],unsigned long position,unsigned short length);
#endif

#ifdef REPORTED_MCU_SN_ENABLE
/**
 * @brief  MCU reports SN result
 * @param[in] {result} 0:Reported successfully  1:Report failed
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void mcu_sn_updata_result(unsigned char result);
#endif

#ifdef WIFI_RESET_NOTICE_ENABLE
/**
 * @brief  Module reset status notification
 * @param[in] {result} Status result
 * @return Null
 * @note   The MCU needs to call the mcu_sn_updata function first, and then process the received result in this function
 */
void wifi_reset_notice(unsigned char result);
#endif


#endif

