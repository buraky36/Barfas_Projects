#ifndef APP_STATE_MACHINE_H
#define APP_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STATE_BOOTING,
    STATE_IDLE,
    STATE_PROGRAMMING,
    STATE_FACTORY_RESET,
    STATE_OTA_UPDATING,
    STATE_DOOR_OPEN,
    STATE_ALARM,
    STATE_USER_PIN_CHANGE,
    STATE_MASTER_ADD_MODE,
    STATE_MASTER_DELETE_MODE,
    STATE_ONLINE_CARD_ENROLL
} app_state_t;

void app_state_machine_init(void);
void app_state_machine_tick(void);

// Used by the logic to change states and trigger outputs
void app_set_state(app_state_t new_state);
void app_trigger_door_open(void);
void app_trigger_door_close(void);
void app_trigger_alarm(void);
void app_trigger_access_denied(void);
uint8_t app_get_working_mode(void);

// Poll inputs explicitly from sensors
void app_poll_sensors(void);

#endif
