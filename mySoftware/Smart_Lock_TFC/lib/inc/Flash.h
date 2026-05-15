
/**
 * @file Flash.h
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This Flash Driver
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


//______________________________ Include Files _________________________________
#include "main.h"
#include "FP_Driver.h"
#include "MFRC522.h"
#include "Tsm12.h"
//==============================================================================


//_____________________________ Generic Objects ________________________________

/**
 * @brief Device-specific information structure
 * @Device_Info_definiton
 */
typedef union 
{
    struct 
    {
        uint8_t RFID_State;
        uint8_t FP_State;
        uint8_t VOICE_State;
        uint8_t LANGUAGE_State;
				uint8_t Default_System;
        uint8_t VERSION[4];
    }Flash_DataBase_ST;

    uint64_t Data[1];
}Flash_DataBase_UN;


/**
 * @brief Address of Generic info and DB @DB_Address_DEFINITION
 */
#define GENERIC_INFO_ADD                (uint32_t)0x0801C800                                // Address of Device info
#define TSM_DB_ADD_START                (GENERIC_INFO_ADD + (uint32_t)16*2)                      // Start Address of TSM DB 
#define TSM_DB_ADD_END                  (TSM_DB_ADD_START + (uint32_t)(8*TSM_DATA_SIZE))    // End Address of TSM DB 
#define RFID_DB_ADD_START               (TSM_DB_ADD_END + (uint32_t)16)                     // Start Address of RFID DB
#define RFID_DB_ADD_END                 (RFID_DB_ADD_START + (uint32_t)(8*RFID_DATA_SIZE))  // End Address of RFID DB
#define TEMP_DB_PASS_ADD_START          (RFID_DB_ADD_END + (uint32_t)16)
#define TEMP_DB_PASS_ADD_END            (TEMP_DB_PASS_ADD_START + (uint32_t)(32*MAX_TEMP_PASS))
#define TEMP_DB_ONLINE_PASS_ADD_START   (TEMP_DB_PASS_ADD_END + (uint32_t)16)
#define TEMP_DB_ONLINE_PASS_ADD_END     (TEMP_DB_ONLINE_PASS_ADD_START + (uint32_t)(32*MAX_TEMP_PASS))
#define DATA_RECORD_ADD_START   				(TEMP_DB_ONLINE_PASS_ADD_END + (uint32_t)16)
#define DATA_RECORD_ADD_END     				(DATA_RECORD_ADD_START + (uint32_t)(32*MAX_TEMP_PASS))

/**
 * @brief Calculate the value of the size of the matrix based on the size of the structure
 * @example  DB_SIZE(FP_DATA_SIZE, FP_DataBase_UN) : FP_DATA_SIZE = 50 , FP_DataBase_UN = 16 -> output = 100 
 */
#define DB_SIZE(st_elements,st_db)          st_elements*(sizeof(st_db)/sizeof(uint64_t))   
/* Data length for generic info */                    
#define FLASH_DATA_SIZE                     1
/* Flash_DataBase variable to store device information.*/
extern Flash_DataBase_UN Flash_DataBase[FLASH_DATA_SIZE];

//==============================================================================


//______________________________ Global Functions ______________________________

void Read_DB(uint32_t flash_Add, uint64_t *db_Add, uint16_t db_size);
HAL_StatusTypeDef erase_page(uint32_t bank, uint32_t nbpages, uint32_t page, uint32_t erase_type);
HAL_StatusTypeDef Write_page(uint32_t flash_Add, uint64_t *db_Add, uint16_t db_size);
void Set_Flash_Write_Active(uint8_t State);
uint8_t Get_Flash_Write_Active();
HAL_StatusTypeDef Erase_and_Write();
void Read_When_Run();
//==============================================================================


//______________________________ Inline Functions ______________________________


//==============================================================================
