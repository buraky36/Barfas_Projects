idf_component_register(SRCS "ota_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES app_state_machine esp_http_client esp_https_ota mbedtls esp-tls app_update)
