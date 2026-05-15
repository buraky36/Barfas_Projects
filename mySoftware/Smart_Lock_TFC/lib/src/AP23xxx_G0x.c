/**
 * @file AP23xxx_G0x.c
 * @author Adem Marangoz (adem.marangoz@barfas.com)
 * @brief This AP23xxx drived by I2C or SPI
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */

//______________________________ Include Files _________________________________
#include "AP23xxx_G0x.h"
#include "Flash.h"
//==============================================================================

//------------------------------- Local Objects --------------------------------
#if(AP23xxx_Interface == AP23xxx_I2C)
  static HAL_StatusTypeDef AP23xxx_I2C_WaitOnFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Flag, FlagStatus Status, uint32_t Timeout, uint32_t Tickstart);
  static HAL_StatusTypeDef AP23xxx_I2C_WaitOnTXISFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart);
  static HAL_StatusTypeDef AP23xxx_I2C_WaitOnSTOPFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart);
  static HAL_StatusTypeDef AP23xx_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);
#endif


/* This variable is used to store the value of the address of the audio file to be played.*/
static uint16_t Voice_Add = 0;

/* This variable is used to determine the playback status of the audio file, whether it is playing or not. */
static uint8_t AP23xxx_Active_State = 0;
uint8_t audio_play_timeout_flag = 0;
//==============================================================================


//------------------------------ External Objects ------------------------------

//==============================================================================


//------------------------------ Global Functions ------------------------------

/**
 * @brief 
 * 
 * @param CMD The type of command to be sent to the IC based on the @AP23xxx_CMD_Definition.
 * @param add The command address
 * @param Timeout Timeout of waiting of send
 * @return HAL_StatusTypeDef    HAL_OK       = 0x00U,
                                HAL_ERROR    = 0x01U,
                                HAL_BUSY     = 0x02U,
                                HAL_TIMEOUT  = 0x03U
 */
HAL_StatusTypeDef AP23xxx_Send(uint8_t CMD, uint16_t add, uint32_t Timeout)
{
  HAL_StatusTypeDef state;
  CMD = (CMD | ((0x0C & CMD) >> 6));
  add &= 0x3FF;
  uint8_t data_16[2] = {CMD , add};
  AP23xxx_Set_NSS(); /* Set Nss pin*/
	HAL_Delay(1); // TODO 20us 
  AP23xxx_Reset_NSS(); /* Reset NSS pin */
  HAL_Delay(1); // TODO 20us 
  state = HAL_SPI_Transmit(&AP23xxx_USED_Interface, (uint8_t *)data_16, 2, Timeout);
  HAL_Delay(2); // TODO 2ms 
  AP23xxx_Set_NSS(); /* Set Nss pin*/
  return state;
}

/**
 * @brief This function is used to stop the power supply to the IC.
 * 
 * @param Timeout Timeout of waiting of send
 * @return HAL_StatusTypeDef    HAL_OK       = 0x00U,
                                HAL_ERROR    = 0x01U,
                                HAL_BUSY     = 0x02U,
                                HAL_TIMEOUT  = 0x03U
 */
HAL_StatusTypeDef AP23xxx_deactive(uint32_t Timeout)
{
  HAL_StatusTypeDef state;
  // TODO wait until the end of last voice group
  AP23xxx_Set_NSS(); /* Set Nss pin*/
  AP23xxx_Reset_NSS(); /* Reset NSS pin */
  HAL_Delay(1); // TODO 20us 
  uint8_t data_16[2] = {PD2_WITH_RAMP_CMD , 0x00};
  state = HAL_SPI_Transmit(&AP23xxx_USED_Interface, (uint8_t *)data_16, 2, Timeout);
  HAL_Delay(2); // TODO 2ms 
  AP23xxx_Set_NSS(); /* Set Nss pin*/
  return state;
}

/**
 * @brief This function is used to provide power to the IC.
 * @param Timeout Timeout duration
 * @return HAL_StatusTypeDef    HAL_OK       = 0x00U,
                                HAL_ERROR    = 0x01U,
                                HAL_BUSY     = 0x02U,
                                HAL_TIMEOUT  = 0x03U
 */
HAL_StatusTypeDef AP23xxx_active(uint32_t Timeout)
{
  HAL_StatusTypeDef state;
  AP23xxx_Set_NSS(); /* Set Nss pin*/
  AP23xxx_Reset_NSS(); /* Reset NSS pin */
  HAL_Delay(1); // TODO 20us 
  uint8_t data_16[2] = {PU1_WITH_RAMP_CMD , 0x00};
  state = HAL_SPI_Transmit(&AP23xxx_USED_Interface, (uint8_t *)data_16, 2, Timeout);
  HAL_Delay(2); // TODO 2ms 
  AP23xxx_Set_NSS(); /* Set Nss pin*/
  return state;

}


/**
 * @brief This function is used to obtain the address of the referenced audio file.
 * 
 * @return uint16_t The address of the audio file
 */
uint16_t Get_Voice_Add()
{
  return Voice_Add;
}


/**
 * @brief This function is used to provide the address of the audio file to be played.
 * 
 * @param Add The address of the audio file
 */
void Set_Voice_Add(uint16_t Add)
{
  Set_AP23xxx_Active_State(1); // set AP23xxx active state to active
  Voice_Add = Add; // voice file address
}


/**
 * @brief This function is used to play the audio file by sending the command
 *  to the IC through the SPI communication protocol.
 * 
 * @return HAL_StatusTypeDef    HAL_OK       = 0x00U,
                                HAL_ERROR    = 0x01U,
                                HAL_BUSY     = 0x02U,
                                HAL_TIMEOUT  = 0x03U
 */
HAL_StatusTypeDef Play_Voice()
{
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t state = Get_AP23xxx_Active_State(); // get AP23xxx active state 
  if(state == 1) // if active
  {
		audio_play_timeout_flag = 1;
    uint16_t voice_add = Get_Voice_Add(); // get voice file address
		if(Flash_DataBase[0].Flash_DataBase_ST.LANGUAGE_State == 2 && voice_add < VOICE_FAIL_PRESSED_BOTTON)
			voice_add += TUR_ENG_CMD_NUM;
    status = AP23xxx_Send(PLAY_CMD, voice_add, 1000); // play voice 		
    Set_AP23xxx_Active_State(2); // set AP23xxx active state to inactive
  }
  return status;
}


/**
 * @brief This function is used to retrieve the playback status of an audio file.
 * 
 * @return uint8_t  1 : Active
 *                  2 : Inactive
 */
uint8_t Get_AP23xxx_Active_State()
{
  return AP23xxx_Active_State;
}


/**
 * @brief This function is used to activate or inactivate the playback of an audio file.
 * 
 * @param State 1 : Active 
 *              2 : Inactive
 */
void Set_AP23xxx_Active_State(uint8_t State)
{
  AP23xxx_Active_State = State;
}
//==============================================================================



//------------------------------- Local Functions ------------------------------
#if(AP23xxx_Interface == AP23xxx_I2C)
/**
  * @brief  Transmits in master mode an amount of data in blocking mode.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  * @param  Timeout Timeout duration
  * @retval HAL status
  */
static HAL_StatusTypeDef AP23xx_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  /* Init tickstart for timeout management*/
    uint32_t tickstart = HAL_GetTick();

    /* Wait until Busy flag to set*/
    if (AP23xxx_I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BUSY, SET, I2C_TIMEOUT_BUSY_FLAG, tickstart) != HAL_OK) 
    {
        return HAL_BUSY;
    }

    /* Config I2C CR2 Registers*/
    I2C_TransferConfig(hi2c, (uint16_t)AP23xxx_Data.CMD[0], 1, I2C_AUTOEND_MODE, I2C_GENERATE_START_WRITE);

    /* Wait until TXIS flag is set */
    if (AP23xxx_I2C_WaitOnTXISFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Write data to TXDR */
      hi2c->Instance->TXDR = AP23xxx_Data.CMD[1];

    /* No need to Check TC flag, with AUTOEND mode the stop is automatically generated */
    /* Wait until STOPF flag is set */
    if (AP23xxx_I2C_WaitOnSTOPFlagUntilTimeout(hi2c, Timeout, tickstart) != HAL_OK)
    {
      return HAL_ERROR;
    }

    /* Clear STOP Flag */
    __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_STOPF);

    /* Clear Configuration Register 2 */
    I2C_RESET_CR2(hi2c);

    return HAL_OK;
}


/**
  * @brief  This function handles I2C Communication Timeout. It waits
  *                until a flag is no longer in the specified status.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  Flag Specifies the I2C flag to check.
  * @param  Status The actual Flag status (SET or RESET).
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef AP23xxx_I2C_WaitOnFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Flag, FlagStatus Status, uint32_t Timeout, uint32_t Tickstart)
{
  /* Wait until flag is set */
  while (__HAL_I2C_GET_FLAG(hi2c, Flag) == Status)
  {
    /* Check for the Timeout */
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((HAL_GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        return HAL_ERROR;
      }
    }
  }
  return HAL_OK;
}


/**
  * @brief  This function handles I2C Communication Timeout for specific usage of TXIS flag.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef AP23xxx_I2C_WaitOnTXISFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  /* Wait until TXIS to reset*/
  while (__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_TXIS) == RESET)
  {
    /* Check for the Timeout */
    if (Timeout != HAL_MAX_DELAY)
    {
      if (((HAL_GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
      {
        return HAL_ERROR;
      }
    }
  }
  return HAL_OK;
}


/**
  * @brief  This function handles I2C Communication Timeout for specific usage of STOP flag.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  Timeout Timeout duration
  * @param  Tickstart Tick start value
  * @retval HAL status
  */
static HAL_StatusTypeDef AP23xxx_I2C_WaitOnSTOPFlagUntilTimeout(I2C_HandleTypeDef *hi2c, uint32_t Timeout, uint32_t Tickstart)
{
  /* Wait until Stop bit to reset*/
  while (__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_STOPF) == RESET)
  {
    /* Check for the Timeout */
    if (((HAL_GetTick() - Tickstart) > Timeout) || (Timeout == 0U))
    {
      return HAL_ERROR;
    }
  }
  return HAL_OK;
}
#endif
//==============================================================================
