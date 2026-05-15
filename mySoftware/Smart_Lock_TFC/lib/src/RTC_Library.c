
#include "RTC_Library.h"
#include "Delay_Library.h"
#include "time.h"
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

RTC_HandleTypeDef *rtc_ptr;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Set_RTC_Date(RTC_DateTypeDef *sDate);
void Set_RTC_Time(RTC_TimeTypeDef *sTime);
void Get_RTC_Time(RTC_TimeTypeDef *gTime);
void Get_RTC_Date(RTC_DateTypeDef *gDate);

void Set_RTC_Alarm(Date_And_Time_Typedef *dt,uint32_t Alarm_Ch);
void Set_RTC_Alarm_For_Next_Second(Date_And_Time_Typedef *dt);


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


// 2000 yılının başlangıcı (Unix Zamanı)
#define EPOCH_YEAR 2000
#define EPOCH_SECONDS 946684800 // 2000-01-01 00:00:00 UTC'den itibaren geçen saniyeler

uint32_t Date_Time_To_UnixTime(Date_And_Time_Typedef *dt)
{
    struct tm timeinfo;

    timeinfo.tm_year = dt->The_Date.Year + 100; // 2000'den sonraki yıl
    timeinfo.tm_mon = dt->The_Date.Month - 1;   // 0-11 arası ay
    timeinfo.tm_mday = dt->The_Date.Date;
    timeinfo.tm_hour = dt->The_Time.Hours;
    timeinfo.tm_min = dt->The_Time.Minutes;
    timeinfo.tm_sec = dt->The_Time.Seconds;

    return (uint32_t)(mktime(&timeinfo) - EPOCH_SECONDS);
}
// Unix Zamanını struct'a dönüştürme fonksiyonu
void UnixTime_To_Date_Time(uint32_t unixTime, Date_And_Time_Typedef *dt)
{
    time_t time = unixTime + EPOCH_SECONDS;
    struct tm *timeinfo = gmtime(&time);

    dt->The_Date.Year = timeinfo->tm_year % 100;  // Yıl (00-99)
    dt->The_Date.Month = timeinfo->tm_mon + 1;   // Ay (1-12)
    dt->The_Date.Date = timeinfo->tm_mday;         // Gün (1-31)
    dt->The_Time.Hours = timeinfo->tm_hour;        // Saat (0-23)
    dt->The_Time.Minutes = timeinfo->tm_min;       // Dakika (0-59)
    dt->The_Time.Seconds = timeinfo->tm_sec;       // Saniye (0-59)
}
uint8_t Get_Weekday_of_Date(RTC_DateTypeDef *sDate)
{
	int h,q,m,k,j,day,month,year;
  day = sDate->Date;
  month = sDate->Month;
  year = sDate->Year;


  if(month == 1)
  {
    month = 13;
    year--;
  }
  if (month == 2)
  {
    month = 14;
    year--;
  }
  q = day;
  m = month;
  k = year % 100;
  j = year / 100;
  h = q + 13*(m+1)/5 + k + k/4 + j/4 + 5*j;
  h = h % 7;
  // Saturday = 0, Sunday = 1 ..... Friday = 6.
  uint8_t Weekday_ret = 0;

  switch(h)
    {
      case 0 : Weekday_ret = RTC_WEEKDAY_SATURDAY; break;
      case 1 : Weekday_ret = RTC_WEEKDAY_SUNDAY; break;
      case 2 : Weekday_ret = RTC_WEEKDAY_MONDAY; break;
      case 3 : Weekday_ret = RTC_WEEKDAY_TUESDAY; break;
      case 4 : Weekday_ret = RTC_WEEKDAY_WEDNESDAY; break;
      case 5 : Weekday_ret = RTC_WEEKDAY_THURSDAY; break;
      case 6 : Weekday_ret = RTC_WEEKDAY_FRIDAY; break;
    }
  return Weekday_ret;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Init_RTC(RTC_HandleTypeDef *rtc,Date_And_Time_Typedef *dt)// first init this
{
	rtc_ptr = rtc;

  /* Calendar ultra-low power mode */
  if (HAL_RTCEx_SetLowPowerCalib(rtc_ptr, RTC_LPCAL_SET) != HAL_OK)
  {
    Error_Handler();
  }


	HAL_RTC_WaitForSynchro(rtc_ptr);
	Get_RTC_Date_And_Time(dt);
	dt->Timeout_Counter = 0;
/*
	HAL_RTC_DeactivateAlarm(rtc_ptr,RTC_ALARM_A);
	HAL_RTC_DeactivateAlarm(rtc_ptr,RTC_ALARM_B);
	*/

}

void RTC_Control_1ms_Timer(Date_And_Time_Typedef *dt)
{
	if(++dt->Timeout_Counter >= 50)
	{
			dt->Timeout_Counter = 0;
			Get_RTC_Date_And_Time(dt);
			//Set_RTC_Alarm_For_Next_Second(dt);
	}

}
void Set_RTC_Date_And_Time(Date_And_Time_Typedef *dt)
{
	Set_RTC_Date(&dt->The_Date);
	Set_RTC_Time(&dt->The_Time);
	Set_RTC_Alarm_For_Next_Second(dt);
}
void Get_RTC_Date_And_Time(Date_And_Time_Typedef *dt)
{
	Get_RTC_Time(&dt->The_Time);
	Get_RTC_Date(&dt->The_Date);

	dt->Raw_UnixTime = Date_Time_To_UnixTime(dt);
}
void Set_RTC_Date(RTC_DateTypeDef *sDate)
{
	sDate->WeekDay = Get_Weekday_of_Date(sDate);
	if (HAL_RTC_SetDate(rtc_ptr, sDate, RTC_FORMAT_BIN) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
	}
}
void Set_RTC_Time(RTC_TimeTypeDef *sTime)
{
	sTime->SubSeconds = 0;
	sTime->TimeFormat = RTC_HOURFORMAT_24;
	sTime->StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(rtc_ptr, sTime, RTC_FORMAT_BIN) != HAL_OK)
	{
		//_Error_Handler(__FILE__, __LINE__);
	}

}
void Get_RTC_Time(RTC_TimeTypeDef *gTime)
{
	HAL_RTC_GetTime(rtc_ptr, gTime, RTC_FORMAT_BIN);
}
void Get_RTC_Date(RTC_DateTypeDef *gDate)
{
	HAL_RTC_GetDate(rtc_ptr, gDate, RTC_FORMAT_BIN);
}
void Set_RTC_Alarm(Date_And_Time_Typedef *dt,uint32_t Alarm_Ch)
{
	RTC_AlarmTypeDef sAlarm = {0};

  memcpy(&sAlarm.AlarmTime,&dt->The_Time,sizeof(sAlarm.AlarmTime));
  sAlarm.AlarmTime.SubSeconds = 0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;

  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY|RTC_ALARMMASK_HOURS|RTC_ALARMMASK_MINUTES;

  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;

  sAlarm.AlarmDateWeekDay = dt->The_Date.Date;
  sAlarm.Alarm = RTC_ALARM_A;

	if (HAL_RTC_SetAlarm_IT(rtc_ptr, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
	{
	    /* Initialization Error */
	    Error_Handler();
	}


}
void Set_RTC_Alarm_For_Next_Second(Date_And_Time_Typedef *dt)// ALARM CALISMADI!!!
{
	Date_And_Time_Typedef New_Date_Time;
	UnixTime_To_Date_Time(dt->Raw_UnixTime + 1, &New_Date_Time);

	New_Date_Time.The_Date.WeekDay = Get_Weekday_of_Date(&New_Date_Time.The_Date);

	HAL_RTC_DeactivateAlarm(rtc_ptr,RTC_ALARM_A);
	Set_RTC_Alarm(&New_Date_Time,RTC_ALARM_A);
}
void RTC_Alarm_Callback(Date_And_Time_Typedef *dt)
{
	dt->Timeout_Counter = 0;
	Get_RTC_Date_And_Time(dt);
	Set_RTC_Alarm_For_Next_Second(dt);
}
