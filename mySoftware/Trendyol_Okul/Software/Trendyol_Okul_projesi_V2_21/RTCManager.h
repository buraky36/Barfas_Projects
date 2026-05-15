#ifndef RTCMANAGER_H
#define RTCMANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define DS1307_ADDRESS 0x68

typedef struct DateTime {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t dayOfWeek; // 1-7
  uint8_t day;
  uint8_t month;
  uint16_t year;
} DateTime;

void rtc_init(void);
bool rtc_begin(void);
DateTime rtc_get_datetime(void);
void rtc_set_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                   uint8_t minute, uint8_t second);
// Buffer must be at least 20 bytes
void rtc_get_datetime_string(char *buffer);

#endif
