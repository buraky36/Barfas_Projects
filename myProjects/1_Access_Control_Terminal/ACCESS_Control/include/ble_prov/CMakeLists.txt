idf_component_register(SRCS "ble_prov.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash wifi_manager storage json nimble)
