idf_component_register(SRCS "ble_manager.c" "ble_server.c" "ble_frame_parser.c"
                    INCLUDE_DIRS "include"
                    REQUIRES bt crypto_manager json)
