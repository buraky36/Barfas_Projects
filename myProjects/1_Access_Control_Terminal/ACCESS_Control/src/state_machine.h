#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

typedef enum { DEVICE_MODE_READER, DEVICE_MODE_CONTROLLER } device_mode_t;

/**
 * @brief Initialize the application state machine
 */
void state_machine_init(void);

/**
 * @brief Switch the device operating mode (READER vs CONTROLLER)
 */
void state_machine_set_mode(device_mode_t mode);

/**
 * @brief Start the state machine task on Core 1
 */
void state_machine_start_task(void);

#endif // STATE_MACHINE_H
