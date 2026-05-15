idf_component_register(SRCS "api_client.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client esp_timer json nv_storage mbedtls)
