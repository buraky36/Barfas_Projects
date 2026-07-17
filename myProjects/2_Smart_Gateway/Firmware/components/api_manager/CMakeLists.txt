idf_component_register(SRCS "api_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_client esp-tls json log crypto_manager)
