#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>


/**
 * @brief System-wide Events
 */
typedef enum {
  EVENT_CARD_SCANNED,
  EVENT_PIN_ENTERED,
  EVENT_QR_SCANNED,
  EVENT_WIEGAND_RX,
  EVENT_OSDP_RX,
  EVENT_WIFI_CONNECTED,
  EVENT_WIFI_DISCONNECTED,
  EVENT_DOOR_OPENED,
  EVENT_DOOR_CLOSED,
  EVENT_REMOTE_UNLOCK,
  /* Add more events as needed */
} event_type_t;

/**
 * @brief Event Payload Structure
 */
typedef struct {
  event_type_t type;
  union {
    uint32_t card_uid; // For EVENT_CARD_SCANNED
    char pin[17];      // For EVENT_PIN_ENTERED (Null-terminated, max 16 chars)
    char qr_data[256]; // For EVENT_QR_SCANNED
                       // Add more payload types as needed
  } payload;
} system_event_t;

/**
 * @brief Initialize the Event Bus (FreeRTOS Queue)
 * @return true if successful
 */
bool event_bus_init(void);

/**
 * @brief Publish an event to the bus from a standard task
 */
bool event_bus_publish(const system_event_t *event);

/**
 * @brief Publish an event to the bus from an ISR
 */
bool event_bus_publish_isr(const system_event_t *event);

/**
 * @brief Wait for and receive the next event from the bus
 *        (Usually called only by the State Machine / Core Logic)
 * @param event Pointer to structure to store received event
 * @param timeout_ticks How long to wait before returning false
 * @return true if an event was received
 */
bool event_bus_receive(system_event_t *event, uint32_t timeout_ticks);

#endif // EVENT_BUS_H
