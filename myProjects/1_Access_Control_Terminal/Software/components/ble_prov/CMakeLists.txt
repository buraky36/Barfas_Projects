idf_component_register(SRCS "ble_prov.c"
                    INCLUDE_DIRS "include"
                    REQUIRES bt json wifi_manager nv_storage nvs_flash hal_io)
