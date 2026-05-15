/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    lock_api.c
 * @author  Tuya Team
 * @version v1.0.7
 * @date    2020.11.9
 * @brief   The door lock function interface is called directly where it is needed to process the logic in the returned data.
 */


#define LOCK_API_GLOBAL

#include "wifi.h"
#include "stdio.h"
#include "string.h"
#include "stm32g0xx_ll_rtc.h"
#include "main.h"
#include "Temp_pass.h"
#include <time.h>

#ifdef LOCK_API_ENABLE

// #define OFFLINE_DYN_PW_ENABLE

/**
 * @brief  MCU requests to obtain temporary cloud password
 * @param  Null
 * @return Null
 * @note   After the MCU actively calls, the temporary password and validity period can be obtained in the temp_pass_handle function
 */
void mcu_get_temp_pass(void)
{ 
    wifi_uart_write_frame(TEMP_PASS_CMD, 0);
}


//ADEM  counter_temp_pass_handle
volatile uint32_t counter_temp_pass_handle;
/**
 * @brief  MCU request temporary password return function
 * @param[in] {suc_flag} Request flag (1:success  0:failure)
 * @param[in] {gl_time} Password validity period Green time (6 digits from low to high, respectively year, month, day, hour, minute, second)
 * @param[in] {pass} Temporary password data (ascll code indicates)
 * @param[in] {pass_len} Temporary password data length
 * @return Null
 * @note   After the MCU actively calls mcu_get_temp_pass, the temporary password and validity period in this function
 */
void temp_pass_handle(unsigned char suc_flag, const unsigned char gl_time[], 
                      const unsigned char pass[], unsigned char pass_len)
{ 
    //#error "Please complete the temporary password processing code yourself and delete the line"
    /*
    suc_flag is whether to obtain the password success flag, 0 means failure, 1 means success
   */
    /*
    gl_time is the green time of the password validity period
    gl_time[0] is the year and 0x00 is the year 2000.
    gl_time[1] is the month, starting from 1 to ending at 12
    gl_time[2] is the date, starting from 1 to 31
    gl_time[3] is the clock, starting from 0 to ending at 23
    gl_time[4] is minutes, starting from 0 to ending at 59
    gl_time[5] is seconds, starting from 0 to ending at 59

    pass points to temporary password data (ascll code), length pass_len
   */
    if (suc_flag == 1) {
        //Successfully obtained temporary password data
    }else {
        //Error getting temporary password data
    }
}

/**
 * @brief  MCU requests dynamic password verification
 * @param[in] {time} Request the current green time (from low to high 6 digits, respectively year, month, day, hour, minute, second)
 * @param[in] {user_pass} The dynamic password entered by the user (from low to high 8 digits, the password is passed in with ASCLL, range '0'-'9')
 * @param[in] {admin_num} Number of administrator password groups (0 ~ 10, if it is 0, the following parameters are invalid)
 * @param[in] {admin_len} Administrator password length (up to 8 bytes)
 * @param[in] {admin_pass} Multiple sets of administrator passwords (multiple sets of passwords are arranged in sequence, length admin_num * admin_len, passed in with ASCLL, range '0'-'9')
 * @return Null
 * @note   The MCU actively calls, after the call is successful, the dynamic password verification result can be obtained in the pass_check_handle function
 */
void dynamic_pass_check(unsigned char time[], unsigned char user_pass[], unsigned char admin_num,
                        unsigned char admin_len, unsigned char admin_pass[])
{ 
    unsigned char length = 0;
    unsigned char i = 0;

    length = set_wifi_uart_buffer(length,(unsigned char *)time, 6);
    length = set_wifi_uart_buffer(length,(unsigned char *)user_pass, 8);
    length = set_wifi_uart_byte(length, admin_num);

    if (admin_num > 0) {
        length = set_wifi_uart_byte(length, admin_len);

        for(i=0;i<admin_num;i++) {
            length = set_wifi_uart_buffer(length,(unsigned char *)(admin_pass+admin_len*i), admin_len);
        }
    }

    wifi_uart_write_frame(PASS_CHECK_CMD, length);
}

/**
 * @brief  The MCU requests to verify the dynamic password return function
 * @param[in] {status} Check mark (0: successful; 1: failure; 2: not activated; 3: length error)
 * @return Null
 * @note   After the MCU initiatively calls dynamic_pass_check successfully, the check result can be obtained in this function
 */
void pass_check_handle(unsigned char status)
{ 
    //#error "Please complete the dynamic password verification result processing code by yourself and delete the line"

    switch (status) {
        case 0: {
            //Pass password check
            break;
        }

        case 1: {
            //Password check failed
            break;
        }

        case 2: {
            //Device not activated
            break;
        }

        case 3: {
            //Data length error
            break;
        }

        default:
        break;
    }
}

/**
 * @brief  The MCU requests multiple sets of temporary passwords in the cloud
 * @param  Null
 * @return Null
 * @note   After the active call of the MCU is completed, the temporary password parameters and the validity period can be obtained in the mul_temp_pass_data function
 */
void mcu_get_mul_temp_pass(void)
{ 
    wifi_uart_write_frame(MUL_TEMP_PASS_CMD, 0);
}


/**
 * @brief  MCU request temporary password return function
 * @param[in] {suc_flag} Request flag (1:success  0:failure)
 * @param[in] {pass_ser} Current group password number (actual number must be +900)
 * @param[in] {pass_valcnt} Number of times the current group password is valid (0:unlimited times  1:one-time)
 * @param[in] {pass_sta} Password current status (0:valid  1:invalid when deleted)
 * @param[in] {gl_start} Password effective date Green time (from low to high 6 digits, respectively year, month, day, hour, minute, second)
 * @param[in] {gl_end} Password expiration date Green time (6 digits from low to high, respectively year, month, day, hour, minute, and second)
 * @param[in] {pass} Temporary password data (ascll code indicates)
 * @param[in] {pass_len} Temporary password data length
 * @return Null
 * @note   After the MCU actively calls mcu_get_mul_temp_pass successfully, the temporary password and validity period of each group can be obtained multiple times in this function
 */
static void mul_temp_pass_data(unsigned char suc_flag, unsigned char pass_ser, 
                               unsigned char pass_valcnt, unsigned char pass_sta, 
                               const unsigned char gl_start[], const unsigned char gl_end[], 
                               const unsigned char pass[], unsigned char pass_len)
{ 
    /*
    suc_flag is whether to obtain the password success flag, 0 means failure, 1 means success
   */
    /*
    pass_ser is the password number
    pass_valcnt is the number of times the password is valid
    pass_sta is the current state of the password

    gl_time is the green time of the password validity period
    gl_time[0] is the year and 0x00 is the year 2000.
    gl_time[1] is the month, starting from 1 to ending at 12
    gl_time[2] is the date, starting from 1 to 31
    gl_time[3] is the clock, starting from 0 to ending at 23
    gl_time[4] is minutes, starting from 0 to ending at 59
    gl_time[5] is seconds, starting from 0 to ending at 59

    pass points to temporary password data (ascll code), length pass_len
   */

    /*
    Note: If multiple groups of passwords are successfully obtained, 
    this function will be entered multiple times until all groups of temporary passwords are obtained; 
    if they fail, they will only be entered once.
   */
    if (suc_flag == 1) {
        //Successfully obtained multiple sets of temporary password data
    }else {
        //Error obtaining multiple sets of temporary password data
    }
}

/**
 * @brief  MCU requests multiple sets of temporary password return function
 * @param[in] {data} Return data
 * @return Null
 * @note   Null
 */
void mul_temp_pass_handle(const unsigned char data[])
{
    unsigned char i = 0;

    unsigned char suc_flag = data[0];
    unsigned char pass_num = data[1];
    unsigned char pass_len = data[2];
    unsigned char offset = 0;

    if (suc_flag == 1) {
        for (i=0;i<pass_num;i++) {
            offset = i*(15+pass_len);
            mul_temp_pass_data(suc_flag, data[offset+3], data[offset+4], data[offset+5], 
                               data+offset+6, data+offset+12, data+offset+18, pass_len);
        }
    }else {
        mul_temp_pass_data(suc_flag, 0, 0, 0, 0, 0, 0, 0);
    }
}

/**
 * @brief  MCU requests to obtain temporary cloud password (with schedule list)
 * @param  Null
 * @return Null
 * @note   After the active call of the MCU is completed, the result can be obtained in the mul_temp_pass_data function
 */
void mcu_get_schedule_temp_pass(void)
{ 
	wifi_uart_write_frame(SCHEDULE_TEMP_PASS_CMD, 0);
}

/**
 * @brief  Schedule list data processing
 * @param[in] {schedule_num} Number of schedule lists
 * @param[in] {schedule_data} schedule list data
 * @return Null
 * @note   After the MCU successfully calls schedule_temp_pass_data,This function can process schedule list data
 */
static void schedule_data_process(unsigned char schedule_num, const unsigned char schedule_data[])
{
    //#error "Please complete the processing code with schedule list and delete the line"
    
    unsigned char i = 0;
    unsigned char offset = 0;
    
    for(i=0;i<schedule_num;i++) {
        /*
        Add schedule list data processing here
        schedule_data[offset]:   0: Not all-day effective, effective in divided time periods    1: All-day effective. The following start time and end time are invalid data
        schedule_data[offset+1]: Start time (hours)
        schedule_data[offset+2]: Start time (minutes)
        schedule_data[offset+3]: End time (hours)
        schedule_data[offset+4]: End time (minutes)
        schedule_data[offset+5]: Weekly cycle (Bit0:Sunday  Bit1:Monday  Bit2:Tuesday  Bit3:Wednesday  Bit4:Thursday  Bit5:Friday  Bit6:Saturday)
                             If the schedule cycle is Sunday and Wednesday, it will be 0x09
        */
		unsigned char weekly_cycle = schedule_data[offset + 5];
		if (weekly_cycle & 0x01) {
		}
		if (weekly_cycle & 0x02) {
		}
		if (weekly_cycle & 0x04) {
		}
		if (weekly_cycle & 0x08) {
		}
		if (weekly_cycle & 0x10) {
		}
		if (weekly_cycle & 0x20) {
		}
		if (weekly_cycle & 0x40) {
		}
        offset += 6;
    }
}

/**
 * @brief  MCU request temporary password return function
 * @param[in] {suc_flag} Request flag (1:success  0:failure)
 * @param[in] {pass_ser} Current group password number (actual number must be +900)
 * @param[in] {pass_valcnt} Number of times the current group password is valid (0:unlimited times  1:one-time)
 * @param[in] {pass_sta} Password current status (0:valid  1:invalid when deleted)
 * @param[in] {gl_start} Password effective date Green time (from low to high 6 digits, respectively year, month, day, hour, minute, second)
 * @param[in] {gl_end} Password expiration date Green time (6 digits from low to high, respectively year, month, day, hour, minute, and second)
 * @param[in] {pass} Temporary password data (ascll code indicates)
 * @param[in] {pass_len} Temporary password data length
 * @param[in] {schedule_num} Number of schedule lists
 * @param[in] {schedule_data} schedule list data
 * @return Null
 * @note   After the MCU actively calls mcu_get_schedule_temp_pass successfully, the temporary password and validity period of each group can be obtained multiple times in this function
 */
static void schedule_temp_pass_data(unsigned char suc_flag, unsigned char pass_ser, 
                               unsigned char pass_valcnt, unsigned char pass_sta, 
                               const unsigned char gl_start[], const unsigned char gl_end[], 
                               const unsigned char pass[], unsigned char pass_len,
                               unsigned char schedule_num, const unsigned char schedule_data[])
{ 
    //#error "Please complete the code for processing temporary password information with schedule list by yourself and delete this line"

    /*
    suc_flag is whether to obtain the password success flag, 0 means failure, 1 means success
   */
    /*
    Note: If multiple groups of passwords are successfully obtained, 
          this function will be entered multiple times until all groups of temporary passwords are obtained; 
          if they fail, they will only be entered once.
   */
    if (suc_flag == 1) {
        //Successfully obtained multiple sets of temporary password data
        /*
        pass_ser is the password number
      pass_valcnt is the number of times the password is valid
      pass_sta is the current state of the password

      gl_time is the green time of the password validity period
      gl_time[0] is the year and 0x00 is the year 2000.
      gl_time[1] is the month, starting from 1 to ending at 12
      gl_time[2] is the date, starting from 1 to 31
      gl_time[3] is the clock, starting from 0 to ending at 23
      gl_time[4] is minutes, starting from 0 to ending at 59
      gl_time[5] is seconds, starting from 0 to ending at 59

      pass points to temporary password data (ascll code), length pass_len
        */
        
    	//Add password data processing here
        
    	schedule_data_process(schedule_num, schedule_data);
        uint16_t temp_index = pass_ser + 900; 
        temp_index &= 0x000F;
        temp_index -= 5; 

		uint8_t temp_pass_byte_array[7];
		for(uint8_t i = 0; i<7; i++) {
			temp_pass_byte_array[i] = pass[i] - 0x30;
		}

		uint64_t st_data = __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_start[0]);
		st_data = (st_data <<8) | (__LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_start[1]));
		st_data = (st_data <<8) | (__LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_start[2]));
		uint64_t st_time = __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_start[3]);
		st_time = (st_time <<8) | __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_start[4]);
		st_time = (st_time <<8) | __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_start[5]);
		st_data = (st_data << 32) | st_time;


		uint64_t en_data = __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_end[0]);
		en_data = (en_data <<8) | (__LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_end[1]));
		en_data = (en_data <<8) | (__LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_end[2]));
		uint64_t en_time = __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_end[3]);
		en_time = (en_time <<8) | __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_end[4]);
		en_time = (en_time <<8) | __LL_RTC_CONVERT_BIN2BCD((unsigned int)gl_end[5]);
		en_data = (en_data << 32) | en_time;

		Save_Temp_Online_pass1(temp_pass_byte_array, st_data, en_data, ONLINE_TIME_PASS, (uint8_t)temp_index, pass_sta);
    }else {
        //Error obtaining multiple sets of temporary password data
    }
}

/**
 * @brief  MCU requests a temporary password (with schedule list) to return function
 * @param[in] {data} Return data
 * @return Null
 * @note   Null
 */
void schedule_temp_pass_handle(const unsigned char data[])
{
    unsigned char i = 0;

    unsigned char suc_flag = data[0];
    unsigned char pass_num = data[1];
    unsigned char pass_len = data[2];
  //  unsigned char pack_num = data[3] & 0x7f;
  //  unsigned char next_pack_flag = data[3] >> 7;
    unsigned char offset = 4;

    if (suc_flag == 1) {
        for (i=0;i<pass_num;i++) {
            schedule_temp_pass_data(suc_flag, data[offset], data[offset+1], data[offset+2], data+offset+3, 
                               data+offset+9, data+offset+15, pass_len, data[offset+15+pass_len],data+offset+15+pass_len+1);
            offset += 15 + pass_len + 1 + 6*data[offset+15+pass_len];
        }
    }else {
        schedule_temp_pass_data(suc_flag, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

#ifdef OFFLINE_DYN_PW_ENABLE
/**
 * @brief  Offline dynamic password
 * @param[in] {green_time} green time
 *              green_time[0] is the year and 0x00 is the year 2000.
 *              green_time[1] is the month, starting from 1 to ending at 12
 *              green_time[2] is the date, starting from 1 to 31
 *              green_time[3] is the clock, starting from 0 to ending at 23
 *              green_time[4] is minutes, starting from 0 to ending at 59
 *              green_time[5] is seconds, starting from 0 to ending at 59
 * @param[in] {pw} Offline dynamic password
 * @param[in] {pw_len} Offline dynamic password length
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the offline_dynamic_password_result function
 */
void offline_dynamic_password(unsigned char green_time[],unsigned char pw[],unsigned char pw_len)
{
    unsigned char length = 0;
    
    length = set_wifi_uart_buffer(length,green_time,6);
    length = set_wifi_uart_byte(length,pw_len);
    length = set_wifi_uart_buffer(length,pw,pw_len);
    
    wifi_uart_write_frame(OFFLINE_DYN_PW_CMD,length);
}

volatile uint8_t faliure_counter = 0;
volatile uint8_t correct_counter = 0;
/**
 * @brief  Offline dynamic password results
 * @param[in] {result_data} result data
 * @return Null
 * @note   The MCU needs to call the offline_dynamic_password function first, and then process the received result in this function
 */
void offline_dynamic_password_result(unsigned char result_data[])
{
    // #error "Please complete the offline dynamic password result processing code yourself and delete the line"
    unsigned char result; //Password correctness
    unsigned char type; //Password Type
    unsigned char decode_len; //Decrypted data length
    unsigned char decode[DECODE_MAX_LEN]; //Decrypted data
    
    result = result_data[0];
    if(0 == result){
        //correct
			TSM_Check = SUCCESS_CHECK;
    }
		else{
        //error
			TSM_Check = FAIL_CHECK;
      return; //In case of error, no follow-up data
    }
    
    type = result_data[1];
    switch(type) {
        case 0:
            //Limited time door open password

        break;
        
        case 1:
            //Single door open password

        break;
        
        case 2:
            //Clear password
        break;
        
        default:
        break;
    }
    
    decode_len = result_data[2];
    my_memcpy(decode,&result_data[3],decode_len);
    
    //Decryption data processing can be added
    
}
#endif

#ifdef PICTURE_UPLOAD_ENABLE
/**
 * @brief  Image upload event status notification
 * @param[in] {event} Event information coding
 * @param[in] {picture} Whether to carry picture information
 * @param[in] {pic_num} Uploaded pictures (<=10)
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the picture_event_state_notice_result function
 */
void picture_event_state_notice(unsigned short event, unsigned char picture, unsigned char pic_num)
{
    unsigned char length = 0;
  
    length = set_wifi_uart_byte(length, (event >> 8) & 0xff);
    length = set_wifi_uart_byte(length, event & 0xff);
    length = set_wifi_uart_byte(length, picture);
    length = set_wifi_uart_byte(length, pic_num);
    
    wifi_uart_write_frame(PICTURE_EVENT_STATE_CMD,length);
}

/**
 * @brief  Image upload event status notification result
 * @param[in] {result} result
 * @return Null
 * @note   The MCU needs to call the picture_event_state_notice function first, and then process the received result in this function
 */
void picture_event_state_notice_result(unsigned char result)
{
    #error "Please complete the Image upload event status notification result processing code yourself and delete the line"
    if (0 == result) {
        //Information received successfully
    }else {
        //Message reception failed
    }
}

/**
 * @brief  upload picture
 * @param[in] {p_time} Time data
 * @param[in] {picture_id} Picture id number
 * @param[in] {pic_total_num} Picture total package
 * @param[in] {pic_num} Current packet number (starting from 0)
 * @param[in] {p_pic_data} Picture data
 * @param[in] {pic_data_len} Picture data length
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the picture_upload_result function
 */
void picture_upload(unsigned char p_time[], unsigned short picture_id, unsigned short pic_total_num, unsigned short pic_num, \
                                            unsigned char p_pic_data[], unsigned char pic_data_len)
{
    unsigned char length = 0;
    
    length = set_wifi_uart_buffer(length, p_time, 7);
    length = set_wifi_uart_byte(length, (picture_id >> 8) & 0xff);
    length = set_wifi_uart_byte(length, picture_id & 0xff);
    length = set_wifi_uart_byte(length, (pic_total_num >> 8) & 0xff);
    length = set_wifi_uart_byte(length, pic_total_num & 0xff);
    length = set_wifi_uart_byte(length, (pic_num >> 8) & 0xff);
    length = set_wifi_uart_byte(length, pic_num & 0xff);
    length = set_wifi_uart_buffer(length, p_pic_data, pic_data_len);
    
    wifi_uart_write_frame(PICTURE_UPLOAD_CMD,length);
}

/**
 * @brief  Image upload result
 * @param[in] {result} result
 * @return Null
 * @note   The MCU needs to call the picture_upload function first, and then process the received result in this function
 */
void picture_upload_result(unsigned char result)
{
    #error "Please complete the Image upload result processing code yourself and delete the line"
    if (0 == result) {
        //Information received successfully
    }else {
        //Information received failed
    }
}

/**
 * @brief  Image upload result feedback
 * @param[in] {p_value} result data
 * @param[in] {data_len} result data length
 * @return Null
 * @note   The MCU needs to process the received results by itself.
 */
void picture_upload_return(unsigned char p_value[], unsigned short data_len)
{
    #error "Please complete the Image upload result feedback processing code yourself and delete the line"
    
    unsigned short pic_id; //Picture id
    unsigned char p_time[6]; //Time of current picture upload
    unsigned char upload_result; //Image upload results
    
    if(9 != data_len) {
        //Data length error
        return;
    }
    
    pic_id = (p_value[0] << 8) | p_value[1];
    my_memcpy(p_time, &p_value[2], 6);
    upload_result = p_value[8];
    
    /*
    //Please handle the feedback results yourself
    //pic_id is the picture id number
    //p_time[6] is the time of the current picture upload, p_time[0]~p_time[5] is the year, month, day, hour, minute, and second
    */
    switch(upload_result) {
        case 0:
            //Picture uploaded successfully
        break;
        
        case 1:
            //Cannot upload successfully if the network is disconnected
        break;
        
        case 2:
            //The picture is too large beyond 40k
        break;
        
        case 3:
            //No picture information of mcu received during timeout
        break;
        
        case 4:
            //Failure for other reasons
        break;
        
        default:break;
    }
    
    unsigned char length = 0;
    
    //Please determine whether there are follow-up pictures to upload, choose to reply data
    
    length = set_wifi_uart_byte(length, 0); //No subsequent image upload
    //length = set_wifi_uart_byte(length, 1); //There is a follow-up picture upload
    
    wifi_uart_write_frame(PICTURE_UPLOAD_RETURN_CMD,length);
}

/**
 * @brief  Get image upload status
 * @param  Null
 * @return Null
 * @note   After the MCU needs to call itself, the result can be processed in the picture_upload_state_get_result function
 */
void picture_upload_state_get(void)
{
    wifi_uart_write_frame(PICTURE_UPLOAD_STATE_GET_CMD, 0);
}

/**
 * @brief  Image upload status get result
 * @param[in] {p_value} Result data
 * @param[in] {data_len} Result data length
 * @return Null
 * @note   The MCU needs to call the picture_upload_state_get function first, and then process the received result in this function
 */
void picture_upload_state_get_result(unsigned char p_value[], unsigned short data_len)
{
    #error "Please complete the Image upload status get result processing code yourself and delete the line"
    unsigned char pic_upload_state; //Image upload status
    unsigned short event_infor; //Event information coding
    unsigned short pic_id; //Picture id
    unsigned char p_time[6]; //Time of current picture upload
    
    if(11 != data_len) {
        //Data length error
        return;
    }
    
    pic_upload_state = p_value[0];
    event_infor = (p_value[1] << 8) | p_value[2];
    pic_id = (p_value[3] << 8) | p_value[4];
    my_memcpy(p_time, &p_value[5], 6);
    
    //Please process the acquired data here
    
}

#endif

#ifdef LOCK_KEEP_ALIVE
/**
 * @brief  Keep alive IO wake
 * @param[in] Null
 * @return Null
 * @note   MCU needs to implement logic code by itself
 */
void keep_alive_awake(void)
{
    #error "Please complete the keep-alive IO wake-up operation code here to wake up the module and delete the line"
    //This function is used for MCU to wake up the module. The wake-up mode is IO low-level wake-up and keeps low after wake-up. Please refer to hardware design for specific IO.
    
}

/**
 * @brief  Get WIFI status
 * @param  Null
 * @return Null
 * @note   After the MCU needs to call by itself, the result can be processed in the get_wifi_state_result function
 */
void mcu_get_wifi_state(void)
{
    wifi_uart_write_frame(GET_WIFI_STATE_CMD, 0);
}

/**
 * @brief  mcu receives WiFi status
 * @param[in] {p_data} Data
 * @return Null
 * @note   MCU needs to call the mcu_get_wifi_state function by itself, and then process the received result in this function
 */
void get_wifi_state_result(const unsigned char p_data[])
{
    #error "Please complete the wifi status processing code by yourself and delete the line"
    switch(p_data[0]) {
        case 0x00:
            //State 1  smartconfig state
        break;
        
        case 0x01:
            //State 2  AP configuration status
        break;
        
        case 0x02:
            //State 3  WIFI is configured but not connected to the router
        break;
        
        case 0x03:
            //State 4  WIFI is configured and connected to the router
        break;
        
        case 0x04:
            //State 5  Connected to the router and connected to the cloud
        break;
        
        case 0x05:
            //State 6  WIFI device is in low power consumption mode
        break;
        
        case 0xff:
            //Invalid state   Initialization incomplete
        break;
        
        default:break;
    }
    
    switch(p_data[1]) {
        case 0x00:
            //inactivated
        break;
        
        case 0x01:
            //activated
        break;
        
        default:break;
    } 
}
#endif


#ifdef PHOTO_LOCK_PICTURE_UPLOAD_ENABLE
/**
 * @brief  MCU sending event triggers snapshot notification
 * @param[in] {event_infor_num} Event information encoding
 * @param[in] {capture_flag} Whether to take a picture
 * @param[in] {capture_type} Snapshot type
 * @param[in] {capture_num} Number of captured pictures(<=6)
 * @param[in] {time_flag} Time flag  0:Without time  1:Local time  2:Green time
 * @param[in] {p_time} Time data
 * @return Null
 * @note   After the MCU needs to call by itself, the result can be processed in the event_trigger_capture_notice_result function.
 */
void mcu_event_trigger_capture_notice(unsigned short event_infor_num, unsigned char capture_flag, unsigned char capture_type,
                                        unsigned char capture_num, unsigned char time_flag, unsigned char p_time[])
{
    unsigned short send_len = 0;
    
    send_len = set_wifi_uart_byte(send_len,EVENT_TRIGGER_CAPTURE_NOTICE_SUBCMD);
    send_len = set_wifi_uart_byte(send_len,(event_infor_num >> 8) & 0xff);
    send_len = set_wifi_uart_byte(send_len,event_infor_num & 0xff);
    send_len = set_wifi_uart_byte(send_len,capture_flag);
    send_len = set_wifi_uart_byte(send_len,capture_type);
    send_len = set_wifi_uart_byte(send_len,capture_num);
    send_len = set_wifi_uart_byte(send_len,time_flag);
    send_len = set_wifi_uart_buffer(send_len, p_time, 6);
    
    wifi_uart_write_frame(PHOTO_LOCK_PICTURE_UPLOAD_CMD, send_len);
}


/**
 * @brief  Event triggers snapshot notification data processing
 * @param[in] {result} Result data
 * @return Null
 * @note   MCU needs to call the mcu_event_trigger_capture_notice function by itself, and then process the received results in this function
 */
static void event_trigger_capture_notice_result(unsigned char result)
{
    #error "Please complete the event-triggered snapshot notification data processing code by yourself, and delete the line"
    
    if(0 == result) {
        //Information received successfully
    }else {
        //Failed to receive information
    }
}

/**
 * @brief  Photo door lock picture upload related functions
 * @param[in] {p_data} Data
 * @return Null
 * @note   
 */
void photo_lock_picture_upload_func(const unsigned char p_data[])
{
    event_trigger_capture_notice_result(p_data[0]); //Event triggers snapshot notification
}
#endif


#endif

