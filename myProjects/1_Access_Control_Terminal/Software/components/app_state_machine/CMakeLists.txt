idf_component_register(SRCS "app_state_machine.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_timer hal_shift_reg mfrc522 hal_touch hal_qr hal_io hal_wiegand nv_storage wifi_manager api_client)
