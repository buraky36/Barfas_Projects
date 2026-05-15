#ifndef RTCMANAGER_H
#define RTCMANAGER_H

#include <Arduino.h>
#include <Wire.h>

#define DS1307_ADDRESS 0x68

struct DateTime {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t dayOfWeek; // 1-7
  uint8_t day;
  uint8_t month;
  uint16_t year;
};

class RTCManager {
public:
  RTCManager();
  bool begin();
  DateTime getDateTime();
  void setDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                   uint8_t minute, uint8_t second);
  String getDateTimeString(); // Format: YYYY-MM-DD HH:MM:SS

private:
  uint8_t decToBcd(uint8_t val);
  uint8_t bcdToDec(uint8_t val);
};

#endif
