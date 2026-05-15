
/**
 * @file Leds.c
 * @author Adem Marangoz (adem.marangoz@barfas.com)
 * @brief This Leds driven
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


//______________________________ Include Files _________________________________
#include "Leds.h"
#include "lock_api.h"
#include "Tsm12.h"

//==============================================================================


//------------------------------ General Objects -------------------------------
RGB_Typedef RGB_State = {0};
static void reset_leds();
static uint32_t starttick = 0;
static uint32_t starttick1 = 0;
static uint32_t blue_time = 0;
extern volatile uint8_t Remotly_control_flag;
uint8_t led_delay_flag = 0;
uint32_t led_delay_count = 0;
uint8_t led_number = 0;
uint8_t led_Blink_count = 0;
uint8_t led_Blink_flag = 0;
uint32_t led_blink_delay = 0;
volatile uint8_t counter_temp_pass = 0;
extern volatile uint8_t failed_password_led_off_flag;
uint8_t get_temp_pass_flag = 0;
//==============================================================================


//______________________________ Global Functions ______________________________
/**
 * @brief reset RGB leds
 */
static void reset_leds()
{
    HAL_GPIO_WritePin(RGB_BLUE_GPIO_Port, RGB_BLUE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RGB_RED_GPIO_Port, RGB_RED_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RGB_GREEN_GPIO_Port, RGB_GREEN_Pin, GPIO_PIN_SET);
}


/**
 * @brief Blinking led according to rgb variable and doing blinking according to _reset_state variable 
 * 
 * @param rgb The color of LED to be run
 * @param _reset_state 1 : reset all leds, 0 : toggle led 
 */
void Set_RGB(RGB_Typedef rgb, uint8_t _reset_state)
{
    if(_reset_state)
    {
        reset_leds();
    }
   switch (rgb)
   {
       case RGB_IDLE:  break;
       case RGB_BLUE:  HAL_GPIO_TogglePin(RGB_GREEN_GPIO_Port, RGB_GREEN_Pin);  RGB_State = RGB_BLUE;   break;
       case RGB_RED:   HAL_GPIO_TogglePin(RGB_RED_GPIO_Port, RGB_RED_Pin);      RGB_State = RGB_RED;    break;
       case RGB_GREEN: HAL_GPIO_TogglePin(RGB_BLUE_GPIO_Port, RGB_BLUE_Pin);    RGB_State = RGB_GREEN;  break;
       default:        break;
   }
}


/**
 * @brief Determine the first color to light
 * 
 * @param rgb The color of LED to be run
 */
void Confi_RGB(RGB_Typedef rgb)
{
    RGB_State = rgb;
    Set_RGB(rgb, 1);
    starttick1 = 0;
    if(rgb == RGB_BLUE){blue_time = 7000;}
}


/**
 * @brief RGB LED color conversion according to the states (durations defined by STANDART_TIME)
 * 
 */

void Run_RGB()
{
    switch (RGB_State)
    {
        case RGB_BLUE:
            if(starttick1 == 0){starttick1 = HAL_GetTick();}
            else 
            {
                if((HAL_GetTick() - starttick1) > blue_time)
								{
									RGB_State = RGB_STOP;
									if(failed_password_led_off_flag == TRUE || Remotly_control_flag )
									{
										Remotly_control_flag = 0;
										failed_password_led_off_flag = 0;
										led_delay_flag = 0;
										led_all_reset();
//										HAL_GPIO_WritePin(KEYPAD_LEDS_PORT, KEYPAD_LEDS_PIN, GPIO_PIN_RESET);
									}
								}
            }
            break;
        case RGB_RED:
            if(starttick1 == 0){starttick1 = HAL_GetTick();}
            else
            {
                if((HAL_GetTick() - starttick1) > STANDART_TIME){
                starttick1 = 0; Set_RGB(RGB_BLUE, 1); blue_time = 2500;}
            }
            break;
        case RGB_GREEN:
            if(starttick1 == 0){starttick1 = HAL_GetTick();}
            else
            {
                
                if((HAL_GetTick() - starttick1) > STANDART_TIME){
                starttick1 = 0; Set_RGB(RGB_BLUE, 1); blue_time = 2500;}
            }
            break;
        case RGB_STOP:
            starttick1 = 0; reset_leds(); RGB_State = RGB_IDLE;    break;
    
        default:
            break;
    }
}

void get_temp_pass()
{
    if(counter_temp_pass)
    {
				if(get_temp_pass_flag)
				{
					mcu_get_gelin_time();
					get_temp_pass_flag = 0;
				}
				else
				{
					mcu_get_schedule_temp_pass();
					get_temp_pass_flag = 1;
				}
        counter_temp_pass = 0;
    }
}
void touch_led(uint8_t wh_led, uint8_t led_State)//which you number touch push
{
	if(!led_State)
	{
		led_delay_count = 0;
		led_delay_flag = 1;
		led_number = wh_led;
		
	}
//	else if(led_delay_flag && led_delay_count >= LED_DELAY)
//	{
//		led_delay_flag = 0;
//	}
	if(led_State)	
	{
		
		led_all_set();
		
//		switch(wh_led)
//		{
//			case BOTTON_0:
//				HAL_GPIO_WritePin(LED11_GPIO_Port, LED11_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_1:
//				HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_2:
//				HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_3:
//				HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_4:
//				HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_5:
//				HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_6:
//				HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_7:
//				HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_8:
//				HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_9:
//				HAL_GPIO_WritePin(LED9_GPIO_Port, LED9_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_START:
//				HAL_GPIO_WritePin(LED10_GPIO_Port, LED10_Pin, GPIO_PIN_SET);
//			break;
//			case BOTTON_HASH:
//				HAL_GPIO_WritePin(LED12_GPIO_Port, LED12_Pin, GPIO_PIN_SET);
//			break;		
//			default:
//			break;
//		}
	}
	else
	{
		switch(wh_led)
		{
			case BOTTON_0:
				led_all_set();
				HAL_GPIO_WritePin(LED11_GPIO_Port, LED11_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_1:
				led_all_set();
				HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_2:
				led_all_set();
				HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_3:
				led_all_set();
				HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_4:
				led_all_set();
				HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_5:
				led_all_set();
				HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_6:
				led_all_set();
				HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_7:
				led_all_set();
				HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_8:
				led_all_set();
				HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_9:
				led_all_set();
				HAL_GPIO_WritePin(LED9_GPIO_Port, LED9_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_START:
				led_all_set();
				HAL_GPIO_WritePin(LED10_GPIO_Port, LED10_Pin, GPIO_PIN_RESET);
			break;
			case BOTTON_HASH:
				led_all_set();
				HAL_GPIO_WritePin(LED12_GPIO_Port, LED12_Pin, GPIO_PIN_RESET);
			break;		
			default:
			break;
		}
	}
}

void led_all_set(void)
{
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin|LED5_Pin|LED11_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin|LED4_Pin|LED6_Pin|LED8_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin|LED9_Pin|LED10_Pin|LED12_Pin, GPIO_PIN_SET);

}

void led_all_reset(void)
{
	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin|LED5_Pin|LED11_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin|LED4_Pin|LED6_Pin|LED8_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin|LED9_Pin|LED10_Pin|LED12_Pin, GPIO_PIN_RESET);

}

void led_blink_start(void)
{
	led_Blink_flag = 1;
	switch(led_Blink_count++)
	{
		case 0:
			HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
		break;
		case 1:
			HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
		break;
		case 2:
			HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
		break;
		case 3:
			HAL_GPIO_TogglePin(LED6_GPIO_Port, LED6_Pin);
		break;
		case 4:
			HAL_GPIO_TogglePin(LED9_GPIO_Port, LED9_Pin);
		break;
		case 5:
			HAL_GPIO_TogglePin(LED12_GPIO_Port, LED12_Pin);
		break;
		case 6:
			HAL_GPIO_TogglePin(LED11_GPIO_Port, LED11_Pin);
		break;
		case 7:
			HAL_GPIO_TogglePin(LED10_GPIO_Port, LED10_Pin);
		break;
		case 8:
			HAL_GPIO_TogglePin(LED7_GPIO_Port, LED7_Pin);
		break;
		case 9:
			HAL_GPIO_TogglePin(LED4_GPIO_Port, LED4_Pin);
		break;
		case 10:
			HAL_GPIO_TogglePin(LED5_GPIO_Port, LED5_Pin);
		break;
		case 11:
			HAL_GPIO_TogglePin(LED8_GPIO_Port, LED8_Pin);
			led_Blink_count = 0;
		break;
		
		default:
    break;
	}
}

void led_blink_stop(void)
{
	led_Blink_flag = 0;
	led_Blink_count = 0;
	led_blink_delay = 0;
	led_delay_flag = 0;	
	led_all_reset();
}

//==============================================================================
