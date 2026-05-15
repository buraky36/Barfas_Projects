
/**
 * @file Battery.h
 * @author Adem Marangoz (adem.marangoz@barfas.com)
 * @brief This Battery driver
 * @version 0.1
 * @date 2023-06-09
 *
 * @copyright Copyright (c) 2022
 *
 */


#ifndef BATTERY_H_
#define BATTERY_H_

//______________________________ Include Files _________________________________
#include "main.h"

//==============================================================================


//_____________________________ Generic Objects ________________________________
/* Battery low level*/
#define BAT_LOW                 0
//==============================================================================


//______________________________ Global Functions ______________________________
void Init_BAT(uint8_t len);
uint8_t Get_BAT();
uint8_t Read_BAT(void);
uint8_t Calc_BAT_Level(void);
//==============================================================================

/********************* MOVING AVERAGE ****************************/

#define MOV_AVG_LENGHT	10

/***** # variable types # ******/

typedef uint16_t data_type;     // Set the Data Type
typedef uint16_t lenght_type;
typedef uint16_t t_size_type;

//typedef struct
//{
//	data_type		*M_Data;
//	t_size_type		 Sample;

//}Measured_Raw_Data;

void movingAvg(data_type *arrToAvg, lenght_type length, t_size_type size);
uint32_t getADCValues(uint32_t samplingTimeUs );
void			measureMicrosecondsStart(void);
uint32_t		measureMicrosecondsGet(void);
void			measureMicrosecondsStop(void);
double			convertRAWToVoltage(uint16_t *buffToConvert, uint16_t startIndex, uint16_t stopIndex);
uint8_t Battery_Measurement(void);

/********************* MOVING AVERAGE ****************************/





#endif
