/**
 * @file buffer.c
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This buffer driver is for RAM data manipulations
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


//______________________________ Include Files _________________________________
#include "buffer.h"
#include <string.h>
#include "Flash.h"
//==============================================================================


//_____________________________ Generic Objects ________________________________


//==============================================================================


//______________________________ Global Functions ______________________________

/**
 * @brief This function is used to determine the index of the empty buffer.
 * 
 * @param db_Add Pointer to a double word variable
 * @param db_size Number of buffer elements
 * @param st_size Size of the variable sent as a double word
 * @return uint32_t empty index of buffer
 *                  0xFFFFFFFF Not found Empty index in buffer
 */
// uint32_t index = Find_Empty_Address((uint64_t*)TSM_DataBase, TSM_DATA_SIZE,  sizeof(TSM_DataBase_UN));
uint32_t Find_Empty_Address(uint64_t *db_Add, uint16_t db_size,  uint8_t st_size)
{
    /* Loop for navigating through array(DB) elements. */
    for(uint32_t i = 0; i < db_size; i++)
    {
        /* Find empty double word */
        if(*db_Add == (uint64_t)EMPTY_DOUBLEWORD_ADD)
        {
            return i; // return empty index of array
        }
        db_Add += (uint8_t)st_size/8; // Increase by the size of the structure.
    }
    return (uint32_t)0xFFFFFFFF;
}

/**
 * @brief This function is used to compare the IDs of two buffers.
 * 
 * @param src Pointer to the incoming data
 * @param des Pointer to the data for the database
 * @param db_size Number of buffer elements of database
 * @param st_size size of structure
 * @param id_size size of id buffer
 * @param id_type ID Role base on @Role_Definition
 * @return uint8_t  return id index if not hasn't found return 0xFF
 *                  
 */
//  uint8_t i = Compare_Id((uint8_t*)&(FP_DataBasetest), (uint8_t*)&(FP_DataBase[0]), 50 , sizeof(FP_DataBase_UN), sizeof(FP_DataBase->FP_DataBase_ST.user_id),USER_ID);
//  uint8_t i = Compare_Id(FP_DataBasetest.FP_DataBase_ST.user_id, FP_DataBase[0].FP_DataBase_ST.user_id, 50 , sizeof(FP_DataBase_UN), sizeof(FP_DataBase->FP_DataBase_ST.user_id),USER_ID);
uint8_t Compare_Id(uint8_t *src, uint8_t *des, uint8_t db_size, uint8_t st_size, uint8_t id_size, ID_Type id_type)
{
    uint8_t src_address = 0;
		uint8_t cmp_ret = 0xFF;
    /* Loop for navigating through array(DB) elements.*/
    for(uint8_t i = 0; i < db_size; i++)
    {
        /* Loop to verify IDs. */
    	for(uint8_t j = 0; j < id_size; j++)
		{
			if(*(src) == *(des)){cmp_ret = i;}
			else{cmp_ret = 0xFF; break;}
				src_address++;
    		des++;
    		src++;
		}

        if(cmp_ret != 0xFF) // if ID correct
        {
            switch (id_type)
            {
                case USER_ID:
                    return cmp_ret;
                    break;
                case ADMIN_ID:
                    /* code */
                    if((*(des)) == ADMIN_ID){return cmp_ret;}
                    break;    
                default:
                    break;
            }
        }
				src -= src_address;
				des -= src_address;
        des += st_size; // Increase by the size of the structure.
				src_address = 0;
    }
    return cmp_ret;
}


/**
 * @brief Delete an ID from the database
 * 
 * @param src Pointer to the incoming data
 * @param des Pointer to the data for the database
 * @param db_size Number of buffer elements of database
 * @param st_size size of structure
 * @param id_size size of id buffer
 * @param id_type ID Role base on @Role_Definition
 * @return uint8_t  0 : successful
 *                  != 0 Fail 
 */
//  uint8_t i = Delete_Id(FP_DataBasetest.FP_DataBase_ST.user_id, FP_DataBase[0].FP_DataBase_ST.user_id, 50 , sizeof(FP_DataBase_UN), sizeof(FP_DataBase->FP_DataBase_ST.user_id),USER_ID);
uint8_t Delete_Id(uint8_t *src, uint8_t *des, uint8_t db_size, uint8_t st_size, uint8_t id_size, ID_Type id_type)
{
		uint8_t rfid_end_admin_control = 0;
		uint8_t src_del_address = 0;
    uint8_t cmp_ret = 1;
		uint16_t admin_control_counter = 0;
		uint8_t *register_start_address = (uint8_t*)des;
	
		if((uint8_t*)&RFID_DataBase[0] == register_start_address)//RFID tek admin silinmesi için touch tek admin silinmiyor asagidaki fonksiyon eklendi.
			rfid_end_admin_control = 1;
		
		register_start_address +=5;
	
    /* Loop for navigating through array(DB) elements.*/
    for(uint8_t i = 0; i < db_size; i++)
    {
        /* Loop to verify IDs. */
        for(uint8_t j = 0; j < id_size; j++)
        {
            if(*(src) == *(des)){cmp_ret = 0;}
			else{cmp_ret = 1; break;}
			  src_del_address++;
    		des++;
    		src++;
        }
        
        if(cmp_ret == 0) // if ID correct
        {
            switch (id_type) // ID access
            {
                case USER_ID:
                    if((*(des)) == USER_ID)
                    {
												des -= src_del_address;
                        memset(des, 0xFF, st_size); // clear element
                        Set_Flash_Write_Active(1);
                        return 0;
                    }
                    break;
                case ADMIN_ID:
                    /* code */
                    if((*(des)) == ADMIN_ID)
                    {
												des -= src_del_address;
											
											for(uint32_t j = 0; j < db_size; j++ ) // admin 1 den fazlami bak
											{
												if( *register_start_address == 2)
													admin_control_counter++;
												register_start_address += 8; 												
											}
											if(admin_control_counter > 1 || rfid_end_admin_control) // admin 1den fazla ise silinebilir 1 ise silme
											{
												memset(des, 0xFF, st_size); // clear element
												Set_Flash_Write_Active(1);
												return 0;
											}
											else
												return 33;
                    }
                    break;    
                default:
                    break;
            }
        }
				else
					cmp_ret = 33;
        src -= src_del_address;
				des -= src_del_address;
        des += st_size; // Increase by the size of the structure.
				src_del_address = 0;
    }
    return cmp_ret;
}

/**
 * @brief Add an ID to the database
 * 
 * @param src Pointer to the data for the database
 * @param des Pointer to the incoming data
 * @param id_size size of id buffer
 * @param id_type ID Role base on @Role_Definition
 */
// Write_To_DB((uint8_t*)&(FP_DataBase[5]), (uint8_t*)&(FP_DataBasetest), sizeof(FP_DataBase->FP_DataBase_ST.user_id), USER_ID);
void Write_To_DB(uint8_t *src, uint8_t *des, uint8_t id_size, ID_Type id_type)
{
    /* Loop for writing data inside a database */
    for (int i = 0; i < id_size; i++) 
    {
        src[i] = des[i];
    }
    
    switch (id_type) // ID access
    {
        case USER_ID:
            *(src + id_size) = 1; // write 1 to id's access  
            break;
        case ADMIN_ID:
            *(src + id_size) = 2; // write 2 to id's access  
            break;
        default:
            break;
    }
    Set_Flash_Write_Active(1);
}

/**
 * @brief Filling the buffer with the value 0xFF
 * 
 * @param src Pointer to the data for the database
 * @param size size of structure
 */
void Fill_Buffers(uint8_t *src, uint8_t size)
{
    memset(src, 0xFF, size*8);
}

//==============================================================================
