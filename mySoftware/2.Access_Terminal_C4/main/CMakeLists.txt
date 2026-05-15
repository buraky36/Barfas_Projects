#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "state_manager.h"
#include "nvs_manager.h"
#include "io_controller.h"
#include "input_manager.h"
#include "protocol_handler.h"
#include "network_manager.h"

void app_main(void)
{
    printf("Access Control Terminal Starting...\n");
    
    // Initialize Core Infrastructure
    nvs_manager_init();
    io_controller_init();
    
    // Initialize Hardware Interfaces
    input_manager_init();
    protocol_handler_init();
    
    // Initialize Networking
    network_manager_init();

    // Start State Manager Task
    state_manager_init();
}
