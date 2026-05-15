
/**
 * @file Tsm12.h
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This TSM12 driver by I2C
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


#ifndef TSM12_H_
#define TSM12_H_


//______________________________ Include Files _________________________________
#include <stdint.h>
#include "main.h"
#include <string.h>
//==============================================================================


//_____________________________ Generic Objects ________________________________

/** 
 * @defgroup A set of definitions through which the user can specify the 
 * activation of some or all sections in the use of I2C and also read the
 */
    #define TSM_Transmit_Polling        1
    #define TSM_Transmit_IT             2
    #define TSM_Transmit_DMA            3
    #define TSM_Receive_Polling         4
    #define TSM_Receive_IT              5
    #define TSM_Receive_DMA             6
    #define TSM_Tx_Type                 TSM_Transmit_Polling
    #define TSM_Rx_Type                 TSM_Receive_Polling

/**
 * @brief @brief The function used in sending and receiving is based on the name of another function name
 */
typedef void (*TSM_Enable)(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin,
                            GPIO_PinState PinState);
typedef HAL_StatusTypeDef (*TSM_Tx_Rx_Poll)(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);
typedef HAL_StatusTypeDef (*TSM_Tx_Rx_IT_DMA)(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size); 


// TSM Delay
#define TSM_Delay(value)       HAL_Delay(value)

// TSM ADDRESS for Write and Read
#define TSM_Write_ADD           0xD0
#define TSM_Read_ADD            0xD1

/**
 * @defgroup TSM Registers
 */
#define TSM_Reg_SENS1           0X02
#define TSM_Reg_SENS2           0X03
#define TSM_Reg_SENS3           0X04
#define TSM_Reg_SENS4           0X05
#define TSM_Reg_SENS5           0X06
#define TSM_Reg_SENS6           0X07
#define TSM_Reg_CTRL1           0X08
#define TSM_Reg_CTRL2           0X09
#define TSM_Reg_REF_RST1        0X0A
#define TSM_Reg_REF_RST2        0X0B
#define TSM_Reg_CH_HOLD1        0X0C
#define TSM_Reg_CH_HOLD2        0X0D
#define TSM_Reg_CAL_HOLD1       0X0E
#define TSM_Reg_CAL_HOLD2       0X0F
#define TSM_Reg_OUTPUT1         0X10
#define TSM_Reg_OUTPUT2         0X11
#define TSM_Reg_OUTPUT3         0X12

/**
 * @defgroup TSM BOTTONS @TSM_BOTTON_DEFITIONS
 * 
 */
#define BOTTON_Num              0x0C
#define BOTTON_0                0X00
#define BOTTON_1                0X01
#define BOTTON_2                0X02
#define BOTTON_3                0X03
#define BOTTON_4                0X04
#define BOTTON_5                0X05
#define BOTTON_6                0X06
#define BOTTON_7                0X07
#define BOTTON_8                0X08
#define BOTTON_9                0X09
#define BOTTON_START            0X0A
#define BOTTON_HASH             0X0B

#define CH_SENS(Ch1, Ch2)       0x##Ch1##Ch2


/**
 * @brief Information specific to how the sensor operates.
 * 
 */
#define TOUCH_EN_PORT           GPIOB
#define TOUCH_EN_PIN            GPIO_PIN_6
#define TOUCH_HANDLE            hi2c1
#define TOUCH_WRITE_TIMEOUT     1000
#define TOUCH_RECEIVE_TIMEOUT   1000
#define TSM_DATA_LEN            5
#define TSM_DATA_SIZE           100
#define TSM_BUFFER_LEN          11

#define _Reset_index_temp_buffer()          index_temp_buffer = 0
#define _Reset_index_tsm_buffer()           index_tsm_buffer = 0
#define _Clear_temp_buffer()                memset(temp_buffer, '\0', 2)                     
#define _Clear_tsm_buffer()                 memset(tsm_buffer, '\0', TSM_BUFFER_LEN)

#define _Reset_And_Clear_Tsm_buffer()       index_tsm_buffer = 0;\
                                            memset(tsm_buffer, 14, TSM_BUFFER_LEN);
#define _Reset_And_Clear_Temp_buffer()      index_temp_buffer = 0;\
                                            memset(temp_buffer, '\0', 2);

/**
 * @brief TSM sensor structure.
 * 
 */
typedef union 
{
    struct 
    {
        uint8_t user_id[TSM_DATA_LEN]; // TSM ID must be 5 value
        uint8_t access; // id access 1 : user, 2 : admin
        uint8_t id_index;
    }TSM_DataBase_ST;

    uint64_t Data[1];
}TSM_DataBase_UN;

extern TSM_DataBase_UN TSM_DataBase[TSM_DATA_SIZE];

/**
 * @brief Statement of the sensitivity of pressing the keypad buttons.
 */
typedef enum
{
    LEVEL_NONE,
    LEVEL_LOW,
    LEVEL_MIDDLE,
    LEVEL_HIGH
}TSM_pressure_t;


/**
 * @brief Operating states or modes of the TSM sensor with 
 * the device algorithm when entering admin mode.
 */
typedef enum
{
    main_menu,
    delete_register_admin,
    delete_register_user,
    system_menu,
    voice_menu,
    language_menu,
    device_menu,
    rfid_device_menu,
    fp_device_menu,
    add_admin_id,
    del_admin_id,
    add_user_id,
    del_user_id,
		sys_reset_hand
}Sys2_menu;


/**
 * @brief Operating states or modes of the TSM sensor with 
 * the device algorithm before entering admin mode.
 */
typedef enum
{
    standby,
    entry_seting_mode,
    setting_mode
}Sys1_menu;


/**
 * @brief TSM sensor structure
 */
typedef struct 
{
    struct 
    {
        I2C_HandleTypeDef *I2c;
        GPIO_TypeDef* Enable_Port;
        uint16_t Enable_Pin;
    }GPIOx;
    TSM_Enable TSM_En;

    #if(TSM_Tx_Type == TSM_Transmit_Polling)
        TSM_Tx_Rx_Poll TSM_Trans_Poll;
    #elif (TSM_Tx_Type == TSM_Transmit_IT)
        TSM_Tx_Rx_IT_DMA TSM_Trans_IT;
    #elif (TSM_Tx_Type == TSM_Transmit_DMA)
        TSM_Tx_Rx_IT_DMA TSM_Trans_DMA;    
    #endif
    
    #if(TSM_Rx_Type == TSM_Receive_Polling)
        TSM_Tx_Rx_Poll TSM_Rec_Poll;
    #elif (TSM_Rx_Type == TSM_Receive_IT)
        TSM_Tx_Rx_IT_DMA TSM_Rec_IT;
    #elif (TSM_Rx_Type == TSM_Receive_DMA)
        TSM_Tx_Rx_IT_DMA TSM_Rec_DMA;
    #endif
}Tsm12_Driver;


extern uint8_t temp_buffer[2];
extern uint8_t tsm_buffer[TSM_BUFFER_LEN];
extern Tsm12_Driver Tsm12_1;
extern Sys1_menu _sys1_menu;
extern Sys2_menu _sys2_menu;
extern uint8_t index_tsm_buffer;
extern uint8_t index_temp_buffer;



//==============================================================================


//______________________________ Global Functions ______________________________

void TSM_Init(uint8_t Sensitivity);
TSM_pressure_t Touch_Pad_check(uint8_t *cmp);
uint8_t TSM_Data_Process();


//==============================================================================



#endif
