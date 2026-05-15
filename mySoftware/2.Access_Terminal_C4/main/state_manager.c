#include "state_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_manager.h"
#include <stdio.h>

#define STATE_MANAGER_QUEUE_SIZE 10

static QueueHandle_t state_queue = NULL;
static system_state_t current_state = STATE_BOOT_INIT;

// Forward declaration of state handlers
static void handle_state_boot_init(void);
static void handle_state_check_factory(void);
static void handle_state_factory_test(void);
static void handle_state_provisioning_check(void);
static void handle_state_provisioning(void);
static void handle_state_normal_operation(void);
static void handle_state_access_evaluating(void);
static void handle_state_access_granted(void);
static void handle_state_access_denied(void);
static void handle_state_alarm(void);

static void state_manager_task(void *pvParameters) {
  system_event_t event;

  // Initial state transition
  current_state = STATE_BOOT_INIT;
  handle_state_boot_init();

  while (1) {
    if (xQueueReceive(state_queue, &event, portMAX_DELAY) == pdPASS) {
      printf("State Manager: Received Event %d\n", event);

      // React to events based on current state
      switch (current_state) {
      case STATE_BOOT_INIT:
        // Usually transitions automatically
        break;
      case STATE_CHECK_FACTORY:
        if (event == EVENT_FACTORY_JIG_DETECTED) {
          current_state = STATE_FACTORY_TEST;
          handle_state_factory_test();
        } else if (event == EVENT_FACTORY_TEST_PASSED) {
          current_state = STATE_PROVISIONING_CHECK;
          handle_state_provisioning_check();
        }
        break;
      case STATE_FACTORY_TEST:
        if (event == EVENT_FACTORY_TEST_PASSED) {
          current_state = STATE_PROVISIONING_CHECK;
          handle_state_provisioning_check();
        }
        break;
      case STATE_PROVISIONING_CHECK:
        if (event == EVENT_PROVISIONING_STARTED) {
          current_state = STATE_PROVISIONING;
          handle_state_provisioning();
        } else if (event == EVENT_PROVISIONING_SUCCESS) {
          current_state = STATE_NORMAL_OPERATION;
          handle_state_normal_operation();
        }
        break;
      case STATE_PROVISIONING:
        if (event == EVENT_PROVISIONING_SUCCESS) {
          current_state = STATE_NORMAL_OPERATION;
          handle_state_normal_operation();
        }
        break;
      case STATE_NORMAL_OPERATION:
        if (event == EVENT_RFID_SCANNED || event == EVENT_PIN_ENTERED ||
            event == EVENT_QR_SCANNED) {
          current_state = STATE_ACCESS_EVALUATING;
          handle_state_access_evaluating();
        } else if (event == EVENT_BUTTON_EXIT) {
          current_state = STATE_ACCESS_GRANTED;
          handle_state_access_granted();
        } else if (event == EVENT_DOOR_FORCED) {
          current_state = STATE_ALARM;
          handle_state_alarm();
        }
        break;
      case STATE_ACCESS_EVALUATING:
        if (event == EVENT_ACCESS_APPROVED) {
          current_state = STATE_ACCESS_GRANTED;
          handle_state_access_granted();
        } else if (event == EVENT_ACCESS_REJECTED) {
          current_state = STATE_ACCESS_DENIED;
          handle_state_access_denied();
        }
        break;
      case STATE_ACCESS_GRANTED:
        if (event == EVENT_DOOR_TIMEOUT) {
          current_state = STATE_NORMAL_OPERATION;
          handle_state_normal_operation();
        } else if (event == EVENT_DOOR_FORCED) {
          // Open Timeout alarm
          current_state = STATE_ALARM;
          handle_state_alarm();
        }
        break;
      case STATE_ACCESS_DENIED:
        // After a short delay/beep, return to normal
        current_state = STATE_NORMAL_OPERATION;
        handle_state_normal_operation();
        break;
      case STATE_ALARM:
        if (event == EVENT_ACCESS_APPROVED) {
          // Authorized reset of alarm
          current_state = STATE_NORMAL_OPERATION;
          handle_state_normal_operation();
        }
        break;
      default:
        break;
      }
    }
  }
}

void state_manager_init(void) {
  state_queue = xQueueCreate(STATE_MANAGER_QUEUE_SIZE, sizeof(system_event_t));
  if (state_queue == NULL) {
    printf("Error: Failed to create state queue\n");
    return;
  }

  xTaskCreate(state_manager_task, "state_manager_task", 4096, NULL, 5, NULL);
  printf("State Manager task initialized.\n");
}

bool state_manager_send_event(system_event_t event) {
  if (state_queue == NULL)
    return false;
  return xQueueSend(state_queue, &event, 0) == pdPASS;
}

system_state_t state_manager_get_state(void) { return current_state; }

// --- State Handlers (Stubs for now) ---

static void handle_state_boot_init(void) {
  printf("Entering State: BOOT INIT\n");
  // Init NVS
  esp_err_t err = nvs_manager_init();
  if (err != ESP_OK) {
    printf("Failed to init NVS: %d\n", err);
  }

  // Check NVS for factory flag
  if (nvs_manager_is_factory_tested()) {
    printf("Factory test already passed previously.\n");
    state_manager_send_event(EVENT_FACTORY_TEST_PASSED);
  } else {
    printf("Device requires factory test.\n");
    // Simulated: wait for jig
    state_manager_send_event(EVENT_FACTORY_JIG_DETECTED);
  }
}

static void handle_state_check_factory(void) {
  printf("Entering State: CHECK FACTORY\n");
}

static void handle_state_factory_test(void) {
  printf("Entering State: FACTORY TEST\n");
}

static void handle_state_provisioning_check(void) {
  printf("Entering State: PROVISIONING CHECK\n");
  // Simulate activated product
  state_manager_send_event(EVENT_PROVISIONING_SUCCESS);
}

static void handle_state_provisioning(void) {
  printf("Entering State: PROVISIONING (AP/BLE Mode)\n");
}

static void handle_state_normal_operation(void) {
  printf("Entering State: NORMAL OPERATION\n");
}

static void handle_state_access_evaluating(void) {
  printf("Entering State: ACCESS EVALUATING\n");
}

static void handle_state_access_granted(void) {
  printf("Entering State: ACCESS GRANTED (Door Open)\n");
  // Simulate door closing after some time
  // In real app, we use a timer and DI-2 (Door Sensor)
  state_manager_send_event(EVENT_DOOR_TIMEOUT);
}

static void handle_state_access_denied(void) {
  printf("Entering State: ACCESS DENIED\n");
}

static void handle_state_alarm(void) { printf("Entering State: ALARM\n"); }
