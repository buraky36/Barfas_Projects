#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


static const char *TAG = "EVENT_BUS";
static QueueHandle_t system_event_queue = NULL;

#define EVENT_QUEUE_SIZE 10

bool event_bus_init(void) {
  if (system_event_queue == NULL) {
    system_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(system_event_t));
    if (system_event_queue == NULL) {
      ESP_LOGE(TAG, "Failed to create Event Queue");
      return false;
    }
    ESP_LOGI(TAG, "Event Bus Initialized");
  }
  return true;
}

bool event_bus_publish(const system_event_t *event) {
  if (system_event_queue == NULL || event == NULL)
    return false;

  // Don't block indefinitely on send, if queue is full discard event or wait
  // briefly
  if (xQueueSend(system_event_queue, event, pdMS_TO_TICKS(10)) != pdTRUE) {
    ESP_LOGE(TAG, "Event Queue Full! Dropped event type: %d", event->type);
    return false;
  }
  return true;
}

bool event_bus_publish_isr(const system_event_t *event) {
  if (system_event_queue == NULL || event == NULL)
    return false;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(system_event_queue, event, &xHigherPriorityTaskWoken) !=
      pdTRUE) {
    // Queue full in ISR, cannot log safely without risking delays depending on
    // ESP-IDF ver
    return false;
  }

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
  return true;
}

bool event_bus_receive(system_event_t *event, uint32_t timeout_ticks) {
  if (system_event_queue == NULL || event == NULL)
    return false;

  if (xQueueReceive(system_event_queue, event, timeout_ticks) == pdTRUE) {
    return true;
  }
  return false;
}
