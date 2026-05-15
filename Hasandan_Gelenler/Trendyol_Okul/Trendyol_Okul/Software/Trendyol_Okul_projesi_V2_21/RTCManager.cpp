#include "RTCManager.h"
#include "Config.h"

RTCManager::RTCManager() {}

bool RTCManager::begin() {
  Serial.println("[RTC] Initializing DS1307...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  Wire.beginTransmission(DS1307_ADDRESS);
  uint8_t error = Wire.endTransmission();

  if (error == 0) {
    Serial.println("[RTC] DS1307 found !");

    // Check for invalid time (e.g. 2000 or 0)
    DateTime now = getDateTime();
    if (now.year < 2025) {
      Serial.println("[RTC] Data Invalid/Lost. Resetting to 2026-01-01");
      setDateTime(2026, 1, 1, 0, 0, 0);
    }
    return true;
  } else {
    Serial.printf("[RTC] ERROR: DS1307 not detected (Error code: %d)\n", error);
    return false;
  }
}

uint8_t RTCManager::decToBcd(uint8_t val) {
  return ((val / 10 * 16) + (val % 10));
}

uint8_t RTCManager::bcdToDec(uint8_t val) {
  return ((val / 16 * 10) + (val % 16));
}

void RTCManager::setDateTime(uint16_t year, uint8_t month, uint8_t day,
                             uint8_t hour, uint8_t minute, uint8_t second) {
  Serial.printf("[RTC] Setting time to: %04d-%02d-%02d %02d:%02d:%02d\n", year,
                month, day, hour, minute, second);
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(1));
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year - 2000));
  uint8_t err = Wire.endTransmission();
  if (err != 0)
    Serial.printf("[RTC] ERROR: Write failed (Code %d)\n", err);
}

DateTime RTCManager::getDateTime() {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0);
  if (Wire.endTransmission() != 0) {
    Serial.println("[RTC] ERROR: Read connection failed");
    return {0, 0, 0, 0, 0, 0, 0};
  }

  Wire.requestFrom(DS1307_ADDRESS, 7);

  DateTime dt;
  if (Wire.available() < 7) {
    Serial.println("[RTC] ERROR: Not enough bytes read");
    return {0, 0, 0, 0, 0, 0, 0};
  }

  dt.second = bcdToDec(Wire.read() & 0x7f);
  dt.minute = bcdToDec(Wire.read());
  dt.hour = bcdToDec(Wire.read() & 0x3f);
  dt.dayOfWeek = bcdToDec(Wire.read());
  dt.day = bcdToDec(Wire.read());
  dt.month = bcdToDec(Wire.read());
  dt.year = bcdToDec(Wire.read()) + 2000;

  // Verbose debug only if needed, otherwise it spams
  // Serial.printf("[RTC] Read: %04d-%02d-%02d %02d:%02d:%02d\n", dt.year,
  // dt.month, dt.day, dt.hour, dt.minute, dt.second);

  return dt;
}

String RTCManager::getDateTimeString() {
  DateTime dt = getDateTime();
  if (dt.year < 2025)
    return String("2026-01-01 00:00:00");

  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", dt.year, dt.month, dt.day,
          dt.hour, dt.minute, dt.second);
  return String(buffer);
}
