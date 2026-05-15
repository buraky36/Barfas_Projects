/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    wifi.h
 * @author  Tuya Team
 * @version v1.0.7
 * @date    2020.11.9
 * @brief   Users don't need to care about the file implementation content.
 */


#ifndef __WIFI_H_
#define __WIFI_H_


//=============================================================================
/*Define constant
  If compilation occurs error:  #40:  expected an identifier    DISABLE = 0, 
  for similar error reporting, you can include a header file in this stm32f1xx.h file.   */
//=============================================================================
#ifndef TRUE
#define         TRUE                1
#endif
//
#ifndef FALSE
#define         FALSE               0
#endif
//
#ifndef NULL
#define         NULL                ((void *) 0)
#endif

// ogulcan: replaced SUCCESS with WIFI_SUCCESS
#ifndef WIFI_SUCCESS
#define         WIFI_SUCCESS             1
#endif

// ogulcan: replaced ERROR with WIFI_ERROR
#ifndef WIFI_ERROR
#define         WIFI_ERROR               0
#endif

#ifndef INVALID
#define         INVALID             0xFF
#endif

// ogulcan: replaced ENABLE with WIFI_ENABLE
#ifndef WIFI_ENABLE
#define         WIFI_ENABLE              1
#endif

// ogulcan: replaced DISABLE with WIFI_DISABLE
#ifndef WIFI_DISABLE
#define         WIFI_DISABLE             0
#endif
//=============================================================================
//dp data point type
//=============================================================================
#define         DP_TYPE_RAW                     0x00        //RAW type
#define         DP_TYPE_BOOL                    0x01        //bool type
#define         DP_TYPE_VALUE                   0x02        //value type
#define         DP_TYPE_STRING                  0x03        //string type
#define         DP_TYPE_ENUM                    0x04        //enum type
#define         DP_TYPE_BITMAP                  0x05        //fault type

//=============================================================================
//WIFI work status
//=============================================================================
#define         SMART_CONFIG_STATE              0x00
#define         AP_STATE                        0x01
#define         WIFI_NOT_CONNECTED              0x02
#define         WIFI_CONNECTED                  0x03
#define         WIFI_CONN_CLOUD                 0x04
#define         LOW_POWER_STATE                 0x05
#define         SMART_AND_AP_STATE              0x06
#define         WIFI_SATE_UNKNOW                0xff
//=============================================================================
//wifi distribution network
//=============================================================================
#define         SMART_CONFIG                    0x0  
#define         AP_CONFIG                       0x1   

//=============================================================================
//wifi reset status
//=============================================================================
#define         RESET_WIFI_ERROR                0
#define         RESET_WIFI_SUCCESS              1

//=============================================================================
//wifi reset result
//=============================================================================
#define         SET_WIFICONFIG_ERROR            0
#define         SET_WIFICONFIG_SUCCESS          1

//=============================================================================
//Whether the device capability is enabled
//=============================================================================
#define         ON                              1                               //Device capability turned on
#define         OFF                             0                               //Device capability turned off

//=============================================================================
//MCU firmware upgrade status
//=============================================================================
#define         FIRM_STATE_UN_SUPPORT           0x00                            //MCU upgrade is not supported
#define         FIRM_STATE_WIFI_UN_READY        0x01                            //Module not ready
#define         FIRM_STATE_GET_ERROR            0x02                            //Cloud upgrade information query failed.
#define         FIRM_STATE_NO                   0x03                            //No upgrade required (no update in the cloud)
#define         FIRM_STATE_START                0x04                            //Need to upgrade, wait for the module to initiate an upgrade operation

//=============================================================================
//How WIFI module and MCU work
//=============================================================================
#define         UNION_WORK                      0x0                             //MCU and WIFI module processing
#define         WIFI_ALONE                      0x1                             //WIFI module self-processing

//=============================================================================
//System working mode
//=============================================================================
#define         NORMAL_MODE             0x00                                    //Normal working condition
#define         FACTORY_MODE            0x01                                    //Factory mode
#define         UPDATE_MODE             0X02                                    //Upgrade mode

//=============================================================================
//download command
//=============================================================================
typedef struct {
    unsigned char dp_id;                        //dp number
    unsigned char dp_type;                      //dp type
} DOWNLOAD_CMD_S;

#pragma anon_unions

#pragma pack(1)

typedef union {
    struct{
        unsigned char   picture_upload          :1;     //Does the device need to support the image upload function?  0: not supported, 1: supported
        unsigned char   communication_mode      :1;     //Which communication method does the device use to upload pictures?  0: indicates serial port, 1: indicates SPI
        unsigned char   operator_door_lock      :1;     //Is the device a carrier door lock?  0: not supported, 1: supported
        unsigned char   reset_state_notice      :1;     //Does the device support module reset status notification?  0: not supported, 1: supported
        unsigned char   useless1                :1;     //Reserved
        unsigned char   useless2                :1;     //Reserved
        unsigned char   useless3                :1;     //Reserved
        unsigned char   useless4                :1;     //Reserved
    };
    unsigned char whole;
}tCAP_equip;

typedef struct {
    unsigned char dp_id;                        //dp number
    unsigned char dp_len;                       //dp data length  ( bool, enum, value type dp need not write this parameter )
    unsigned char dp_bool_val;                  //bool type dp value
    unsigned char dp_enum_val;                  //enum type dp value
    unsigned int  dp_value_val;                 //value type dp value
    unsigned int  dp_fault_bitmap;              //fault type dp value
    unsigned char *dp_str_val;                  //string type dp value
    unsigned char *dp_raw_val;                  //raw type dp value
    
}t_DP_NODE;

#pragma pack()


#include "protocol.h"
#include "system.h"
#include "mcu_api.h"
#include "stm32g0xx.h"
#include "stm32g0xx_ll_usart.h"

#ifdef LOCK_API_ENABLE
#include "lock_api.h"
#endif


#endif
