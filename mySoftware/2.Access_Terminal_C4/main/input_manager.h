#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "esp_err.h"

/**
 * @brief Initialize all input peripherals (MFRC522, TSM12, XR1300)
 *        and start the input polling/interrupt task.
 */
esp_err_t input_manager_init(void);

#endif // INPUT_MANAGER_H
