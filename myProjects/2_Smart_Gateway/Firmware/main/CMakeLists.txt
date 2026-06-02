idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES config_manager wifi_manager ble_manager mqtt_manager web_server)
