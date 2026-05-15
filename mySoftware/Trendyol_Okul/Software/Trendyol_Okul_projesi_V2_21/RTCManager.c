#include "RTCManager.h"
#include "wrappers.h"
#include "Config.h" // For PIN definitions if needed
#include <stdio.h>

static uint8_t decToBcd(uint8_t val) {
    return ((val / 10 * 16) + (val % 10));
}

static uint8_t bcdToDec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

void rtc_init(void) {
    // No specific struct initialization needed for C module pattern
}

bool rtc_begin(void) {
    sys_i2c_begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    sys_i2c_begin_transmission(DS1307_ADDRESS);
    if (sys_i2c_end_transmission() == 0) {
        return true;
    }
    return false;
}

DateTime rtc_get_datetime(void) {
    DateTime dt = {0,0,0,0,0,0,0};
    
    sys_i2c_begin_transmission(DS1307_ADDRESS);
    sys_i2c_write(0); // Set register pointer to 0
    sys_i2c_end_transmission();
    
    sys_i2c_request_from(DS1307_ADDRESS, 7);
    
    if (sys_i2c_available() >= 7) {
        dt.second = bcdToDec(sys_i2c_read() & 0x7F);
        dt.minute = bcdToDec(sys_i2c_read());
        dt.hour   = bcdToDec(sys_i2c_read() & 0x3F); // 24h format assumption
        dt.dayOfWeek = bcdToDec(sys_i2c_read());
        dt.day    = bcdToDec(sys_i2c_read());
        dt.month  = bcdToDec(sys_i2c_read());
        dt.year   = bcdToDec(sys_i2c_read()) + 2000;
    }
    
    return dt;
}

void rtc_set_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                   uint8_t minute, uint8_t second) {
    sys_i2c_begin_transmission(DS1307_ADDRESS);
    sys_i2c_write(0);
    sys_i2c_write(decToBcd(second));
    sys_i2c_write(decToBcd(minute));
    sys_i2c_write(decToBcd(hour));
    sys_i2c_write(decToBcd(1)); // Day of week (placeholder, mostly ignored or should be calc)
    sys_i2c_write(decToBcd(day));
    sys_i2c_write(decToBcd(month));
    sys_i2c_write(decToBcd(year % 100));
    sys_i2c_end_transmission();
}

void rtc_get_datetime_string(char *buffer) {
    DateTime dt = rtc_get_datetime();
    // Format: YYYY-MM-DD HH:MM:SS
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", 
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}
