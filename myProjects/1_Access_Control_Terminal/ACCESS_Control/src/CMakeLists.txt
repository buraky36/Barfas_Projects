FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)
FILE(GLOB_RECURSE include_sources ${CMAKE_SOURCE_DIR}/include/*.c)
list(APPEND app_sources ${include_sources})

set(INCLUDE_DIRS 
    "../include"
    "../include/auth_engine"
    "../include/ble_prov"
    "../include/comm_osdp"
    "../include/comm_wiegand"
    "../include/device_config"
    "../include/door_control"
    "../include/event_bus"
    "../include/keypad"
    "../include/led_ui"
    "../include/qr_reader"
    "../include/rfid"
    "../include/server_comm"
    "../include/storage"
    "../include/system_monitor"
    "../include/wifi_manager"
)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS ${INCLUDE_DIRS}
                       REQUIRES nvs_flash bt)
