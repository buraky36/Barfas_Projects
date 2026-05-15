/*
 * Battery.c
 *
 *  Created on: Jul 8, 2024
 *      Author: Hasan
 */


//______________________________ Include Files _________________________________
#include "battery.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "protocol.h"
//==============================================================================




//_____________________________ Generic Objects ________________________________

extern ADC_HandleTypeDef hadc1;                             // Call HAL ADC Instance
#define BAT_INS     hadc1                                   // This has to be the same as above
#define BAT_BUF_LEN         20                              // MAX Buffer length
#define MIN_BAT_LEV         4200//2100  kontol edilecek     // Minimum voltage(3V) value
#define MAX_BAT_LEV         5500//3400                      // Maximum voltage(6V) value
#define BAT_SLOPE_COEF      ((MAX_BAT_LEV - MIN_BAT_LEV)/100) // Slope coefficient

#ifdef BAT_VOLTAGE_SOFTWARE_ENABLE
#define BAT_DRAINED_LIMIT			4.20f		// Percent level of the battery to be considered as drained for Tuya smart app
#define BAT_LOW_LIMIT					4.60f		// Percent level of the battery to be considered as low for Tuya smart app
#define BAT_MID_LIMIT					4.90f		// Percent level of the battery to be considered as medium for Tuya smart app
#define BAT_HIGH_LIMIT				7.50f		// Percent level of the battery to be considered as high for Tuya smart app
#endif

#define Resistor1  					270000   ///27k %1
#define Resistor2  					330000   ///33k %1 //Gerilim bölücü üzerine düsen V      Vout=(Vin*R2)/(R1+R2)
#define RT12							 (Resistor1+Resistor2)

static uint8_t Buf_len = 0;                                 // Sample Rate of Buffer
static uint8_t battPercentLevel = 0;                        // Battery Percentage
static uint16_t BAT_Buf[BAT_BUF_LEN] = {0xFFFFFFFF};        // Battery buffer
static uint8_t Cal_BAT(uint32_t buf);

volatile uint32_t adc_read_value = 0;
volatile float read_voltage_value = 0;

volatile uint16_t adc_sequentialBuffer[300];
bool adcConvertCompletedFlag = false;
uint16_t adc_buffer[1];

uint32_t measuredCount = 0;

volatile double adc_temp = 0;

volatile uint8_t Bat_Level_Value = 0;
volatile uint32_t Bat_Percent_Value = 0;

//==============================================================================


//______________________________ Global Functions ______________________________

/**
 * @brief This function is used to determine the number of samples to take
 *
 * @param len number of samples
 */
void Init_BAT(uint8_t len)
{
    Buf_len = len;
}


/**
 * @brief This function is used to calculate the battery percentage
 *
 * @param buf The current value of the battery
 */
static uint8_t Cal_BAT(uint32_t buf)
{
    static uint8_t counter  = 0;
    BAT_Buf[counter++] = buf;
    if(counter == Buf_len)
    {
        uint32_t temp = 0;
        for(uint8_t i = 0; i < Buf_len; i++)
        {
            temp += BAT_Buf[i]/Buf_len;
        }
				movingAvg(BAT_Buf, 3, 20);
				read_voltage_value = (((float)temp)/4095)*3.00f;
        if(temp > MAX_BAT_LEV){temp = MAX_BAT_LEV;}
        else if(temp < MIN_BAT_LEV){temp = MIN_BAT_LEV;}
        battPercentLevel = (uint8_t)((temp - (uint32_t)MIN_BAT_LEV)/(uint32_t)BAT_SLOPE_COEF);
        counter = 0;
    }
    return counter;
}

/**
 * @brief This function is used to return the battery value as a percentage
 *
 * @return uint8_t Battery value in percentage 100 ~ 0
 */
uint8_t Get_BAT()
{
    return battPercentLevel;
}


/**
 * @brief This function is used to read the value of the battery
 * @note this function reads adc value one time
 *       The adc value must be read based on the Buf_len variable
 */
#ifdef BAT_VOLTAGE_SOFTWARE_ENABLE
uint32_t adc_buf_count = 0;
uint8_t Read_BAT(void)
{
//	if(HAL_ADC_Init(&hadc1) != HAL_OK) HAL_ADC_Init(&hadc1);
//	HAL_Delay(10);
	adc_buf_count = getADCValues(4000); //1000us
	movingAvg( adc_sequentialBuffer, 10,  adc_buf_count);

	for(uint16_t i = 0; i < adc_buf_count-9; i++)
	{
		adc_temp += ((float)adc_sequentialBuffer[i]);
	}

	adc_temp /= adc_buf_count-9;
//	adc_temp = ((adc_temp/4095)*3.3000f);
	adc_temp = ((((adc_temp/4095)*3.3000f)*(RT12))/Resistor2)+0.25f; //27k-33k  //0.25V offset olcum esnasinda AAA pil gerilim düsümü icin
//	if(HAL_ADC_DeInit(&hadc1) != HAL_OK) HAL_ADC_DeInit(&hadc1);
	if(adc_temp > 0)
	{
		return 1;
	}
	else
		return 0;
}
#endif

/**
 * @brief This function is used to calculate the level of the battery according to Tuya standard
 * @return	0:	HIGH
 * 			1:	MEDIUM
 * 			2:	LOW
 * 			3:	COMPLETELY DRAINED
 */
#ifdef BAT_VOLTAGE_SOFTWARE_ENABLE
uint8_t Calc_BAT_Level(void)
{
	if(adc_temp <= BAT_DRAINED_LIMIT)
		return 3;
	else if((adc_temp > BAT_DRAINED_LIMIT) && (adc_temp <= BAT_LOW_LIMIT))
		return 2;
	else if((adc_temp > BAT_LOW_LIMIT) && (adc_temp <= BAT_MID_LIMIT))
		return 1;
	else if((adc_temp > BAT_MID_LIMIT) && (adc_temp <= BAT_HIGH_LIMIT))
		return 0;
}
#endif
//==============================================================================

/********************* MOVING AVERAGE Start ****************************/
extern TIM_HandleTypeDef htim3;
void movingAvg(uint16_t *arrToAvg, lenght_type length, t_size_type size)
{

	data_type arrToAvg_value[size] ;
	arrToAvg_value[0] = arrToAvg[0];

	for(int i=1; i < length; i++){

		arrToAvg[0] += arrToAvg[i] ;
	}
	arrToAvg[0] /= length;

	for(uint32_t i=1; i <= (size - length); i++){

		arrToAvg_value[i] = arrToAvg[i] ;
		arrToAvg[i] = ((arrToAvg[i-1] * length ) - arrToAvg_value[i-1] + arrToAvg[(i-1) + length]) / length;

	}
	for(uint32_t k=(size - length+1); k < size; k++){
		arrToAvg[k] = 0;
	}
}

/********************* MOVING AVERAGE Stop ****************************/



/*Get RAW ADC values and measure total time to get values for 5 channel*/
uint32_t getADCValues( uint32_t samplingTimeUs )
{
	measuredCount = 0;
	/*variable for total measurement time*/
	__IO uint32_t elapsedTimeTick = 0;

	/*clear adc_SequentialBuffer*/
	memset(adc_sequentialBuffer, 0, sizeof(adc_sequentialBuffer));

	/*Start 1us timer tick*/
	measureMicrosecondsStart();

	/*take measurements while total elapsed time < samplingTime*/
	while((elapsedTimeTick < samplingTimeUs))
	{
		/*prepare ADC trigger flag*/
		adcConvertCompletedFlag = false;

		/*start ADC linked to DMA*/
		HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, 1);

		bool state = adcConvertCompletedFlag;

		/*wait for ADC conversion to finish or timeout*/
		while((state != true) && (elapsedTimeTick < samplingTimeUs))
		{
			elapsedTimeTick = measureMicrosecondsGet();
			state = adcConvertCompletedFlag;
		}
		elapsedTimeTick = measureMicrosecondsGet();
	}


	HAL_ADC_Stop_DMA(&hadc1);
	measureMicrosecondsStop();

	return measuredCount;
}

/*
Start timer 15.
Timer 15 generates an interrupt at every 1 micro seconds.
In this interrupt, microsecondTick value increments by 1.
Overflow of microsecondTick happens if this timer couldn't stop (which souldn't) for 71 minutes.
*/
void measureMicrosecondsStart(void)
{
	htim3.Instance->CNT = 0;
	/*TIM3 interrupt Init*/
	HAL_TIM_Base_Start(&htim3);
}

/*Stop timer 15*/
void measureMicrosecondsStop(void)
{
	HAL_TIM_Base_Stop(&htim3);
}

/*return counter value of TIM 15 (microseconds)*/
uint32_t measureMicrosecondsGet(void)
{
	return htim3.Instance->CNT;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{

	if(measuredCount < 200)
	{
		measuredCount++;
		adc_sequentialBuffer[measuredCount -1] = adc_buffer[0];
		adcConvertCompletedFlag = true;
	}
}

uint8_t Batt_Measurement_OK = 0;
uint8_t Battery_Measurement(void)
{

	__HAL_RCC_TIM3_CLK_ENABLE();

	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

	if(HAL_ADC_Init(&hadc1) != HAL_OK) HAL_ADC_Init(&hadc1);
	ADC->CCR |= ADC_CCR_VREFEN; // ADC VREF ENABLE

	ADC_ChannelConfTypeDef sConfig = {0};
	sConfig.Channel = ADC_CHANNEL_7;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	/* USER CODE BEGIN ADC1_Init 2 */
	HAL_ADCEx_Calibration_Start(&hadc1);

	if(Read_BAT())
	{
		Batt_Measurement_OK = 1;
	}
	else
		Batt_Measurement_OK = 0;

	Bat_Level_Value = Calc_BAT_Level();

    switch (Bat_Level_Value)
    {
        case 0:
        	Bat_Percent_Value = 100;
            break;
        case 1:
        	Bat_Percent_Value = 75;
            break;
        case 2:
        	Bat_Percent_Value = 50;
            break;
        case 3:
        	Bat_Percent_Value = 25;
            break;
        default:
            break;
    }

//	mcu_dp_value_update(DPID_RESIDUAL_ELECTRICITY,Bat_Percent_Value); //VALUE type data report;

	ADC->CCR &= ~ADC_CCR_VREFEN; // ADC VREF DISABLE
	if(HAL_ADC_DeInit(&hadc1) != HAL_OK) HAL_ADC_DeInit(&hadc1);

	HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
	__HAL_RCC_DMA1_CLK_DISABLE();

	__HAL_RCC_TIM3_CLK_DISABLE();

	return Batt_Measurement_OK;
}

