#ifndef IO_CONTROLLER_H
#define IO_CONTROLLER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Initialize all GPIOs for inputs and outputs
 */
esp_err_t io_controller_init(void);

/**
 * @brief Set the door relay state
 * @param open true to engage relay (open door), false to disengage
 */
void io_controller_set_relay(bool open);

/**
 * @brief Beep the buzzer
 * @param ms Duration in milliseconds
 */
void io_controller_beep(uint32_t ms);

/**
 * @brief Set status LED state
 * @param state 0=Off, 1=Green (Granted), 2=Red (Denied/Alarm)
 */
void io_controller_set_led(uint8_t state);

/**
 * @brief Get door sensor state (DI-2)
 * @return true if closed, false if open
 */
bool io_controller_get_door_sensor(void);

#endif // IO_CONTROLLER_H
