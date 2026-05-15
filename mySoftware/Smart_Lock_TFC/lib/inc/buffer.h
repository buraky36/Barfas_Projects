/**
 * @file buffer.h
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This buffer Driver
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


#ifndef BUFFER_H_
#define BUFFER_H_

//______________________________ Include Files _________________________________
#include "main.h"

//==============================================================================


//_____________________________ Generic Objects ________________________________

/**
 * @brief This variable is used to determine the role that will be written to the buffer.
 * @Role_Definition
 */
typedef enum
{
    USER_ID = 1,
    ADMIN_ID
}ID_Type;


/* Empty Double Word */
#define EMPTY_DOUBLEWORD_ADD            0xFFFFFFFFFFFFFFFF


//==============================================================================


//______________________________ Global Functions ______________________________

uint32_t Find_Empty_Address(uint64_t *db_Add, uint16_t db_size, uint8_t st_size);
uint8_t Compare_Id(uint8_t *src, uint8_t *des, uint8_t db_size, uint8_t st_size, uint8_t id_size, ID_Type id_type);
uint8_t Delete_Id(uint8_t *src, uint8_t *des, uint8_t db_size, uint8_t st_size, uint8_t id_size, ID_Type id_type);
void Write_To_DB(uint8_t *src, uint8_t *des, uint8_t id_size, ID_Type id_type);
void Fill_Buffers(uint8_t *src, uint8_t size);

//==============================================================================


#endif

