#ifndef DOOR_CONTROL_H
#define DOOR_CONTROL_H

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Initialize door control pins and configurations
 */
void door_control_init(void);

/**
 * @brief Unlock the door for a given duration
 * @param duration_ms Duration in milliseconds (e.g., 3000ms)
 */
void door_control_unlock(uint32_t duration_ms);

/**
 * @brief Immediately lock the door
 */
void door_control_lock(void);

/**
 * @brief Check the current status of the magnetic door sensor
 * @return true if the door is physically open, false if closed
 */
bool door_control_is_open(void);

#endif // DOOR_CONTROL_H
