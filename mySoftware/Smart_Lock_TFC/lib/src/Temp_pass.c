/**
 * @file Temp_pass.c
 * @author Adem Marangoz (adem.marangoz95@gmail.com)
 * @brief This temprory password Driver
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


//______________________________ Include Files _________________________________
#include "Temp_pass.h"
#include "Flash.h"
#include "buffer.h"
#include "stm32g0xx_ll_rtc.h"
//==============================================================================


//_____________________________ Generic Objects ________________________________

/**
 * @brief It is used to calculate the days, months, and years during the conversion process from epoch to real-time.
 * 
 */
static unsigned short days[4][12] =
{
    {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
    { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
    { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
    {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};

/* Temporary code database */
Temp_pass_St Temp_Pass_DB[MAX_TEMP_PASS] = {0};
Temp_pass_St Temp_Online_Pass_DB[MAX_TEMP_PASS] = {0};
static void epoch_to_date_time(Temp_pass_St* date_time,unsigned int start_epoch, unsigned int end_epoch);
static uint8_t find_empty_index();
static uint32_t convert_date_value(uint32_t date);
static uint64_t Cur_Date_time();
static uint8_t compare_id(uint8_t *des, uint8_t *src);
RTC_HandleTypeDef *rtc_ptr;
RTC_DateTypeDef *gDate;
//==============================================================================


//______________________________ Global Functions ______________________________

/**
 * @brief This function is used to read the current time and date values from the RTC (Real-Time Clock).
 * 
 * @return uint64_t Current Date and Time base on RTC 
 */
static uint64_t Cur_Date_time()
{
	uint32_t time = LL_RTC_TIME_Get(RTC);
    uint32_t data = LL_RTC_DATE_Get(RTC);
    data = convert_date_value(data);
    uint64_t cur_date_time1 = ((uint64_t)data << 32) | time;
    return cur_date_time1;
}

volatile uint64_t tttest;
/**
 * @brief It is used to find one of the indices that can be written to in the Temporary Pass Database.
 * 
 * @return uint8_t if Full : 0xFF
 *                 if there is an empty one return index of Temporary Pass Database
 */
static uint8_t find_empty_index()
{
    volatile uint64_t cur_date_time = Cur_Date_time();
    tttest = cur_date_time + 1;
    for(uint8_t i = 0; i < MAX_TEMP_PASS; i++)
    {
        if(Temp_Pass_DB[i].end_date_time < cur_date_time) {return i;}
    }
    return 0xFF;
}


volatile uint64_t tttest;
/**
 * @brief It is used to find one of the indices that can be written to in the Temporary Pass Database.
 * 
 * @return uint8_t if Full : 0xFF
 *                 if there is an empty one return index of Temporary Pass Database
 */
static uint8_t find_empty_index_online()
{
    
    volatile uint64_t cur_date_time = Cur_Date_time();
    tttest = cur_date_time + 1;
    for(uint8_t i = 0; i < MAX_TEMP_PASS; i++)
    {
        if(Temp_Online_Pass_DB[i].end_date_time < cur_date_time) {return i;}
    }
    return 0xFF;
}

/**
 * @brief This function uses the date format from 0xWWDDMMYY and converts it to 0xWWYYMMDD.
 * 
 * @param date Date Value
 * @return uint32_t converted date value
 */
static uint32_t convert_date_value(uint32_t date)
{
    date &= 0x00FFFFFF;
    uint32_t temp_year = 0x000000FF & date;
    uint32_t temp_day = date >> 16;
    date = (date & 0x0000FF00);
    date = (date | temp_day | (temp_year << 16));
    return date;
}


/**
 * @brief It is used to save a temporary Pass along with the start and end times for the Pass's validity.
 * 
 * @param Pass Pointer to Pass buffer 
 * @param st_time Start Time come as unix from Tuya
 * @param en_time end Time come as unix from Tuya
 * @param _temp_pass_type Password type base on @def_PASS_TYPE
 */
void Save_Temp_Online_pass(uint8_t *Pass, uint32_t st_time, uint32_t en_time, Temp_Pass_Type _temp_pass_type)
{
    uint8_t index = find_empty_index_online();
    if(index != 0xFF)
    {
        epoch_to_date_time(Temp_Online_Pass_DB + index, st_time, en_time);
        Temp_Online_Pass_DB[index].pass_type = _temp_pass_type;
        for(uint8_t i = 0; i < ONLINE_PASS_LEN; i++) {Temp_Online_Pass_DB[index].Pass[i] = *(Pass + i);}
    }else { /* When the DB is Full */}
}


/**
 * @brief It is used to save a temporary Pass along with the start and end times for the Pass's validity.
 * 
 * @param Pass Pointer to Pass buffer 
 * @param st_time Start Time come as unix from Tuya
 * @param en_time end Time come as unix from Tuya
 * @param _temp_pass_type Password type base on @def_PASS_TYPE
 */
void Save_Temp_Online_pass1(uint8_t *Pass, uint64_t st_time, uint64_t en_time, Temp_Pass_Type _temp_pass_type, uint8_t index, uint8_t validation)
{
    if((validation == 0))
    {
      Temp_Online_Pass_DB[index].start_date_time = st_time;
    	Temp_Online_Pass_DB[index].end_date_time = en_time;
    	Temp_Online_Pass_DB[index].pass_type = _temp_pass_type;
      for(uint8_t i = 0; i < ONLINE_PASS_LEN; i++) {Temp_Online_Pass_DB[index].Pass[i] = *(Pass + i);}
//		  Set_Flash_Write_Active(1);
    }else
    {
			Temp_Online_Pass_DB[index].start_date_time = 0;
    	Temp_Online_Pass_DB[index].end_date_time = 0;
    	Temp_Online_Pass_DB[index].pass_type = 0;
      for(uint8_t i = 0; i < ONLINE_PASS_LEN; i++) {Temp_Online_Pass_DB[index].Pass[i] = 0;}
    }



    // uint8_t index = find_empty_index_online();
    // if(index != 0xFF)
    // {
    // 	Temp_Online_Pass_DB[index].start_date_time = st_time;
    // 	Temp_Online_Pass_DB[index].end_date_time = en_time;
    // 	Temp_Online_Pass_DB[index].pass_type = _temp_pass_type;
    //     for(uint8_t i = 0; i < ONLINE_PASS_LEN; i++) {Temp_Online_Pass_DB[index].Pass[i] = *(Pass + i);}
    // }else { /* When the DB is Full */}
}

/**
 * @brief It is used to save a temporary Pass along with the start and end times for the Pass's validity.
 *
 * @param Pass Pointer to Pass buffer
 * @param st_time Start Time come as unix from Tuya
 * @param en_time end Time come as unix from Tuya
 * @param _temp_pass_type Password type base on @def_PASS_TYPE
 */
void Save_Temp_pass(uint8_t *Pass, uint32_t st_time, uint32_t en_time, Temp_Pass_Type _temp_pass_type)
{
    uint8_t index = find_empty_index();
    if(index != 0xFF)
    {
        epoch_to_date_time(Temp_Pass_DB + index, st_time, en_time);
        Temp_Pass_DB[index].pass_type = _temp_pass_type;
        for(uint8_t i = 0; i < PASS_LEN; i++) {Temp_Pass_DB[index].Pass[i] = *(Pass + i);}
//        Set_Flash_Write_Active(1); // Active write to Flash
    }else 
			{ /* When the DB is Full */}
}


/**
 * @brief It is used to save a temporary Pass along with the start and end times for the Pass's validity.
 *
 * @param Pass Pointer to Pass buffer
 * @param st_time Start Time come as unix from Tuya
 * @param en_time end Time come as unix from Tuya
 * @param _temp_pass_type Password type base on @def_PASS_TYPE
 */
void Save_Temp_pass1(uint8_t *Pass, uint64_t st_time, uint64_t en_time, Temp_Pass_Type _temp_pass_type)
{
    uint8_t index = find_empty_index();
    if(index != 0xFF)
    {
    	  Temp_Pass_DB[index].start_date_time = st_time;
    	  Temp_Pass_DB[index].end_date_time = en_time;
        Temp_Pass_DB[index].pass_type = _temp_pass_type;
        for(uint8_t i = 0; i < PASS_LEN; i++) {Temp_Pass_DB[index].Pass[i] = *(Pass + i);}
//        Set_Flash_Write_Active(1); // Active write to Flash
    }else 
			{ /* When the DB is Full */
				Temp_Pass_DB[index].start_date_time = 0;
				Temp_Pass_DB[index].end_date_time = 0;
				Temp_Pass_DB[index].pass_type = 0;
				for(uint8_t i = 0; i < PASS_LEN; i++) {Temp_Pass_DB[index].Pass[i] = 0;}
			}
}

/**
 * @brief This function is used to perform a check on the entered code through TSM with the temporary code database.
 * 
 * @param tsm_input pointer to TSM buffer
 * @return uint8_t  if valid : return index of  temp password
 *                  if not valid : return 0xFF 
 */
uint8_t check_valid_pass(uint8_t *tsm_input)
{
    volatile uint64_t cur_date_time = Cur_Date_time();
    uint8_t cmp_ret = 0xFF;
    uint64_t temp_date = 0x00FFFFFF00000000 & cur_date_time;
    uint64_t temp_time = 0x0000000000FFFFFF & cur_date_time;
    for(uint8_t i = 0; i < MAX_TEMP_PASS; i++)
    {
        /* This condition is used to determine if the LIMITED_TIME_PASS code has been used within one day of its creation.*/
        if(Temp_Pass_DB[i].pass_type == LIMITED_TIME_PASS && Temp_Pass_DB[i].used == 0) 
        {
            if(temp_date > ((Temp_Pass_DB[i].start_date_time) & 0x00FFFFFF00000000))  
            {
                if(temp_time <= ((Temp_Pass_DB[i].start_date_time) & 0x0000000000FFFFFF))
                {
                    cmp_ret = Compare_Id(tsm_input, (uint8_t*)Temp_Pass_DB, 4, MAX_TEMP_PASS, PASS_LEN, USER_ID); // comapre with DB
                    if(cmp_ret != 0xFF) {Temp_Pass_DB[i].used = 1; return cmp_ret; Set_Flash_Write_Active(1);}
                }else {Temp_Pass_DB[i].end_date_time = 0x0000000000000000; Temp_Pass_DB[i].pass_type = ONE_TIME_PASS;}
            }else if(temp_date == ((Temp_Pass_DB[i].start_date_time) & 0x00FFFFFF00000000))
            {
                    cmp_ret = Compare_Id(tsm_input, (uint8_t*)Temp_Pass_DB, 4, MAX_TEMP_PASS, PASS_LEN, USER_ID); // comapre with DB
                    if(cmp_ret != 0xFF) {Temp_Pass_DB[i].used = Temp_Pass_DB[i].used + 1; return cmp_ret;}
            }
        }else if(Temp_Pass_DB[i].pass_type == ONE_TIME_PASS) /* It is used to compare ONE_TIME_PASS passwords and also LIMITED_TIME_PASS passwords that are used only once after their creation within one day.*/
        {
           if(cur_date_time < Temp_Pass_DB[i].end_date_time)
           {
                cmp_ret = Compare_Id(tsm_input, (uint8_t*)Temp_Pass_DB, 4, MAX_TEMP_PASS, PASS_LEN, USER_ID);
                if(cmp_ret != 0xFF) {Temp_Pass_DB[i].used = Temp_Pass_DB[i].used + 1; return cmp_ret;}
           }
        }

        if(Temp_Online_Pass_DB[i].pass_type == ONLINE_TIME_PASS)
        {
            if(cur_date_time >= (Temp_Online_Pass_DB[i].start_date_time))
            {
                if(cur_date_time <= Temp_Online_Pass_DB[i].end_date_time)
                {
                    cmp_ret = compare_id(tsm_input, (uint8_t*)(Temp_Online_Pass_DB + i));
                    if(cmp_ret != 0xFF) {Temp_Online_Pass_DB[i].used = 1; return cmp_ret;}
                    // cmp_ret = Compare_Id(tsm_input, (uint8_t*)Temp_Online_Pass_DB, 4, MAX_TEMP_PASS , PASS_LEN, USER_ID); // comapre with DB
                    // if(cmp_ret != 0xFF) {Temp_Online_Pass_DB[i].used = 1; return cmp_ret;}
                }
            }
        }
    }

    return cmp_ret;
}


static uint8_t compare_id(uint8_t *des, uint8_t *src)
{
    uint8_t cmp_ret = 0x0;
    for(uint8_t i; i < ONLINE_PASS_LEN; i++)
    {
        if(*(des + i) != *(src + i)){cmp_ret = 0xFF; break;}
        else{cmp_ret = i;}
    }
    return cmp_ret;
}


/**
 * @brief it is used to convert unix timestamp to normal time
 * 
 * @param date_time pointer to Database 
 * @param start_epoch start time as unix 
 * @param end_epoch end time as unix
 */
static void epoch_to_date_time(Temp_pass_St* date_time,unsigned int start_epoch, unsigned int end_epoch)
{
    // Start Time Cal
    uint8_t _seconds = __LL_RTC_CONVERT_BIN2BCD(start_epoch%60); start_epoch /= 60;
    uint8_t _minutes = __LL_RTC_CONVERT_BIN2BCD(start_epoch%60); start_epoch /= 60;
    uint8_t _hours = __LL_RTC_CONVERT_BIN2BCD(start_epoch%24);   start_epoch /= 24;
    uint32_t _time = (_hours << 16) | (_minutes  << 8) | (_seconds);
    
    unsigned int years = start_epoch/(365*4+1)*4; 
    start_epoch %= 365*4+1;
    
    unsigned int year;
    for (year=3; year>0; year--)
    {
        if (start_epoch >= days[year][0])
            break;
    }
    unsigned int month;
    for (month=11; month>0; month--)
    {
        if (start_epoch >= days[year][month])
            break;
    }

    uint32_t _year  = __LL_RTC_CONVERT_BIN2BCD(years+year-30);
    uint32_t _month = __LL_RTC_CONVERT_BIN2BCD(month+1);
    uint32_t _day   = __LL_RTC_CONVERT_BIN2BCD(start_epoch-days[year][month]+1);
    uint32_t _date  = (_year << 16) | (_month << 8) | (_day);
    date_time->start_date_time = ((uint64_t)_date << 32) | (_time);

    // End Time Cal   
    _seconds = __LL_RTC_CONVERT_BIN2BCD(end_epoch%60); end_epoch /= 60;
    _minutes = __LL_RTC_CONVERT_BIN2BCD(end_epoch%60); end_epoch /= 60;
    _hours   = __LL_RTC_CONVERT_BIN2BCD(end_epoch%24); end_epoch /= 24;
    _time = (_hours << 16) | (_minutes  << 8) | (_seconds);

    years = end_epoch/(365*4+1)*4; 
    end_epoch %= 365*4+1;
    
    for (year=3; year>0; year--)
    {
        if (end_epoch >= days[year][0])
            break;
    }

    for (month=11; month>0; month--)
    {
        if (end_epoch >= days[year][month])
            break;
    }
    _year  = __LL_RTC_CONVERT_BIN2BCD(years+year-30);
    _month = __LL_RTC_CONVERT_BIN2BCD(month+1);
    _day   = __LL_RTC_CONVERT_BIN2BCD(end_epoch-days[year][month]+1);
    _date = (_year << 16) | (_month << 8) | (_day);
    date_time->end_date_time = ((uint64_t)_date << 32) | (_time);
}

//==============================================================================

