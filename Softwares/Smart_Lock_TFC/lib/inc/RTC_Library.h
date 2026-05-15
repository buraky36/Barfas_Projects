
#ifndef ___RTC_LIBRARY_H___
#define ___RTC_LIBRARY_H___

#include "main.h"

typedef struct
{
	RTC_DateTypeDef The_Date;
	RTC_TimeTypeDef The_Time;
	uint32_t				Raw_UnixTime;
	uint32_t				Timeout_Counter;

}Date_And_Time_Typedef;


void Init_RTC(RTC_HandleTypeDef *rtc,Date_And_Time_Typedef *dt);
void RTC_Control_1ms_Timer(Date_And_Time_Typedef *dt);

void Set_RTC_Date_And_Time(Date_And_Time_Typedef *dt);
void Get_RTC_Date_And_Time(Date_And_Time_Typedef *dt);

void RTC_Alarm_Callback(Date_And_Time_Typedef *dt);

#endif //___RTC_LIBRARY_H___
