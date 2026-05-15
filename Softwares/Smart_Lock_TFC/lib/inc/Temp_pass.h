/**
 * @file Temp_pass.h
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This temprory password Driver
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


#ifndef TEMP_PASS_H_
#define TEMP_PASS_H_

//______________________________ Include Files _________________________________
#include "main.h"
//==============================================================================


//_____________________________ Generic Objects ________________________________
/**
 * @brief Type of Temprory Password
 * @def_PASS_TYPE
 */
typedef enum
{
    WAIT_TEMP_PASS,
    ONE_TIME_PASS,
    LIMITED_TIME_PASS,
    ONLINE_TIME_PASS
}Temp_Pass_Type;


#define MAX_TEMP_PASS           10 // Temp Password Length
#define PASS_LEN                10 // Temp password DB Size
#define ONLINE_PASS_LEN         7 // Temp password DB Size

/**
 * @brief Structure of Temp password 
 */
typedef struct 
{
    uint8_t Pass[10]; // password
    uint64_t start_date_time; // start time of temp password
    uint64_t end_date_time; // end time of temp password
    Temp_Pass_Type pass_type; // type of temp password
    uint8_t used;
}Temp_pass_St;


/**
 * @brief Structure of Temp password 
 */
typedef struct 
{
    uint8_t Pass[7]; // password
    uint64_t start_date_time; // start time of temp password
    uint64_t end_date_time; // end time of temp password
    Temp_Pass_Type pass_type; // type of temp password
    uint8_t used;
}Online_Temp_pass_St;


extern Temp_pass_St Temp_Pass_DB[MAX_TEMP_PASS];
extern Temp_pass_St Temp_Online_Pass_DB[MAX_TEMP_PASS];

//==============================================================================


//______________________________ Global Functions ______________________________
void Save_Temp_pass(uint8_t *Pass, uint32_t st_time, uint32_t en_time, Temp_Pass_Type _temp_pass_type);
void Save_Temp_Online_pass(uint8_t *Pass, uint32_t st_time, uint32_t en_time, Temp_Pass_Type _temp_pass_type);
void Save_Temp_pass1(uint8_t *Pass, uint64_t st_time, uint64_t en_time, Temp_Pass_Type _temp_pass_type);
void Save_Temp_Online_pass1(uint8_t *Pass, uint64_t st_time, uint64_t en_time, Temp_Pass_Type _temp_pass_type, uint8_t index, uint8_t validation);
uint8_t check_valid_pass(uint8_t *tsm_input);
//==============================================================================


#endif

