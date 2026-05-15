#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "state_manager.h"

void app_main(void)
{
    printf("Access Control Terminal Starting...\n");
    // State Manager görevini (Task) başlat
    state_manager_init();
}
