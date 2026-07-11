idf_component_register(SRCS "mqtt_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt ble_manager config_manager json esp_app_format app_update)
