idf_component_register(SRCS "server_comm.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_client json nvs_flash esp_timer)
