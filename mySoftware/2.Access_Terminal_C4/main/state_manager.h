#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief System states for the Access Control Terminal
 */
typedef enum {
  STATE_BOOT_INIT,          // System initialization
  STATE_CHECK_FACTORY,      // Check if factory passed
  STATE_FACTORY_TEST,       // Hardware factory tests
  STATE_PROVISIONING_CHECK, // Check if activated
  STATE_PROVISIONING,       // AP/BLE provisioning mode
  STATE_NORMAL_OPERATION,   // Disconnected/Connected Idle mode
  STATE_ACCESS_EVALUATING,  // Evaluating credentials
  STATE_ACCESS_GRANTED,     // Door opened
  STATE_ACCESS_DENIED,      // Invalid credential
  STATE_ALARM               // Forced door, etc.
} system_state_t;

/**
 * @brief Events that trigger state transitions
 */
typedef enum {
  EVENT_NONE,
  EVENT_FACTORY_JIG_DETECTED,
  EVENT_FACTORY_TEST_PASSED,
  EVENT_FACTORY_TEST_FAILED,
  EVENT_PROVISIONING_STARTED,
  EVENT_PROVISIONING_SUCCESS,
  EVENT_RFID_SCANNED,
  EVENT_PIN_ENTERED,
  EVENT_QR_SCANNED,
  EVENT_ACCESS_APPROVED,
  EVENT_ACCESS_REJECTED,
  EVENT_DOOR_TIMEOUT,
  EVENT_DOOR_FORCED,
  EVENT_DOOR_CLOSED,
  EVENT_BUTTON_EXIT
} system_event_t;

/**
 * @brief Initialize the state manager and start its FreeRTOS task
 */
void state_manager_init(void);

/**
 * @brief Send an event to the state manager queue
 * @param event The event to send
 * @return true if successfully queued
 */
bool state_manager_send_event(system_event_t event);

/**
 * @brief Get the current system state
 * @return Current system_state_t
 */
system_state_t state_manager_get_state(void);

#endif // STATE_MANAGER_H
