/**
 * @file common.h
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


#ifndef COMMON_H_
#define COMMON_H_

//______________________________ Include Files _________________________________
#include "main.h"
#include <stdint.h>
#include "buffer.h"
//==============================================================================


//_____________________________ Generic Objects ________________________________

/**
 * @brief This structure is used to determine whether the admin's code is correct 
 * or incorrect.
 * 
 */
typedef enum
{
    ADMIN_ID_IDLE,
    ADMIN_ID_FAIL,
    ADMIN_ID_SECCESFUL
}Admin_id_typedef;

typedef struct 
{
   uint16_t admin_count;
	 uint16_t user_count;	
        
}Smart_Lock;

extern Admin_id_typedef admin_id;

#define MAX_ADMIN 10
#define MAX_USER  100
#define MAX_USER_FINGERPRINT 40

//==============================================================================


//______________________________ Global Functions ______________________________
void system_reset(void);
void success_admin_login(void);
void fail_ademin_login(void);
void active_dis_device(uint8_t _active);
void registrtion_id(uint64_t *db_add, uint16_t db_size, uint8_t st_size, uint8_t *des, uint8_t id_size, ID_Type id_type);
void delete_id(uint8_t *src, uint8_t *des, uint8_t db_size, uint8_t st_size, uint8_t id_size, ID_Type id_type);
//==============================================================================



//=========================== NOTE ===========================
// ADEM REMOVE COMMENT

#endif