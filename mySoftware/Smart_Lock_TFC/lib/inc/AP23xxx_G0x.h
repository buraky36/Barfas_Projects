
/**
 * @file AP23xxx_G0x.h
 * @author Adem Marangoz (adem.marangoz@barfas.com)
 * @brief This AP23xxx drived by I2C or SPI
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


/**
 * --------------------------------- GUIDE API ---------------------------------
 *  Example :
 *  AP23xxx_Create_Frame(PLAY_CMD, 0x01);
 *  AP23xxx_Send(HAL_MAX_DELAY);
 */

#ifndef AP23XXX_G0X_H_
#define AP23XXX_G0X_H_
//______________________________ Include Files _________________________________
#include "main.h"

//==============================================================================

//------------------------------ General Objects -------------------------------

/* Disable and Enable AP23xxx Chip @Activate_AP23xxx_definition */
#define AP23xxx_Disable         0
#define AP23xxx_Enable          1

/* AP23xxx Chip Interface @AP23xxx_Interface_definition */
#define AP23xxx_SPI             0
#define AP23xxx_I2C             1


/* Can choose AP23xxx using Interface using definition @AP23xxx_Interface_definition */
#define AP23xxx_Interface       AP23xxx_SPI
/* Can Disable and Enable This Drive by This define whick base on @Activate_AP23xxx_definition */
#define Activate_AP23xxx        AP23xxx_Enable

#if(AP23xxx_Interface == AP23xxx_SPI)
  /* Include hal spi structure */
  extern SPI_HandleTypeDef hspi2;
  /* Define Hal SPI structure */
  #define AP23xxx_USED_Interface            hspi2
  /* Define NSS Port */
  #define AP23xxx_NSS_Port                  GPIOB
  /* Define NSS Pin*/
  #define AP23xxx_NSS_Pin                   GPIO_PIN_12
  /* Define Reset Nss Pin function */
  #define AP23xxx_Reset_NSS()               HAL_GPIO_WritePin(AP23xxx_NSS_Port, AP23xxx_NSS_Pin, GPIO_PIN_RESET)
  /* Define Set Nss Pin function */
  #define AP23xxx_Set_NSS()                 HAL_GPIO_WritePin(AP23xxx_NSS_Port, AP23xxx_NSS_Pin, GPIO_PIN_SET)
#elif(AP23xxx_Interface == AP23xxx_I2C)
  /* Include hal i2c structure */
  extern I2C_HandleTypeDef hi2c2;
  /* define Hal I2C structure */
  #define AP23xxx_USED_Interface hi2c2
  /* define Send 2 byte by I2C to AP23xxx function */ 
  #define AP23xxx_2Byte_Send(buf,Timeout)   AP23xx_I2C_Master_Transmit(&AP23xxx_USED_Interface, (uint8_t *)buf, 2, Timeout)
#endif


/**
 * @defgroup AP23xxx Command group @AP23xxx_CMD_Definition
 */
#define LOAD_CMD                        0x94
#define PLAY_CMD                        0x98
#define PU1_WITHOUT_RAMP_CMD            0xA4
#define PU1_WITH_RAMP_CMD               0xA8
#define PD1_WITHOUT_RAMP_CMD            0xB1
#define PD2_WITH_RAMP_CMD               0xB8
#define VOL_CMD                         0x44
#define VOL_PLUS_CMD                    0x48
#define VOL_MINUS_CMD                   0x54
#define PAUSE_CMD                       0x64
#define RESUME_CMD                      0x68
#define Donot_Care                      0x00


/**
 * @defgroup voice file address @VOICE_ADD_DEFINITION 
 * 
 */

#define TUR_ENG_CMD_NUM    0x1B //27 ADD

#define VOICE_OPEN_DOOR                 0x00 //0 NUMBER
#define VOICE_WORNG_ENTRY               0x01
#define VOICE_SYSTEM_LOCKED             0x02
#define VOICE_OPERATION_SUCCESSFUL      0x03
#define VOICE_ENTRY_ADMIN_ID            0x04
#define VOICE_MAIN_MENU                 0x05
#define VOICE_REG_DEL_ADMIN             0x06
#define VOICE_REG_DEL_USER              0x07
#define VOICE_DEVICE_SETTING_MENU       0x08
#define VOICE_SOUND_OK_CANCEL           0x09
#define VOICE_TURKISH_ENGLISH           0x0A
#define VOICE_RFID_FP_MANU              0x0B
#define VOICE_ACTIVE_MENU               0x0C
#define VOICE_LOW_BATTERY               0x0D
#define VOICE_OK_CANCEL                 0x0E
#define VOICE_ENTER_THE_ID              0x0F
#define VOICE_REMOTE_CONTROLLING        0x10
#define VOICE_ONLOI                     0x11 //yok
#define VOICE_ONLOI_SMART_WORLD         0x12
#define VOICE_MAX_REGISTRATION          0x13
#define VOICE_REGISTERED_USER           0x14
#define VOICE_READ_AGAIN                0x15 
#define VOICE_CONNECTING					      0x16
#define VOICE_CONNECTING_ERROR          0x17 // yok
#define VOICE_TRY_AGAIN                 0x18  
#define VOICE_REJECTED                  0x19
#define VOICE_CLOSE_DOOR                0x1A //27 NUMBER

///************* ## SOUND ENG FILES SIZE 0x1B to 0x35 ##  ******************//

#define VOICE_FAIL_PRESSED_BOTTON       0x36 // Beep Fail
#define VOICE_PRASSED_BOTTON      			0x37 // Beep OK
#define VOICE_SILENCE                   0x38 // 24 REGISTER
#define Max_Group                       1024 // Maximum Number of Group Sound


//#define VOICE_OPEN_DOOR                 0x20 // Decimal 32


/**
 * @brief Check Passed Command to AP23xxx
 */
#define _IS_CMD_Correct_AP23xxx(CMD)    (((CMD) == LOAD_CMD)||\
                                        ((CMD) == PLAY_CMD)||\
                                        ((CMD) == PU1_WITHOUT_RAMP_CMD)||\
                                        ((CMD) == PU1_WITH_RAMP_CMD)||\
                                        ((CMD) == PD1_WITHOUT_RAMP_CMD)||\
                                        ((CMD) == PD2_WITH_RAMP_CMD)||\
                                        ((CMD) == VOL_CMD)||\
                                        ((CMD) == VOL_PLUS_CMD)||\
                                        ((CMD) == VOL_MINUS_CMD)||\
                                        ((CMD) == PAUSE_CMD)||\
                                        ((CMD) == RESUME_CMD))
//==============================================================================

//______________________________ Global Functions ______________________________
HAL_StatusTypeDef AP23xxx_Send(uint8_t CMD, uint16_t add, uint32_t Timeout);
uint16_t Get_Voice_Add();
void Set_Voice_Add(uint16_t Add);
HAL_StatusTypeDef Play_Voice();
HAL_StatusTypeDef AP23xxx_active(uint32_t Timeout);
HAL_StatusTypeDef AP23xxx_deactive(uint32_t Timeout);
uint8_t Get_AP23xxx_Active_State();
void Set_AP23xxx_Active_State(uint8_t State);
//==============================================================================

#endif
