FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)
idf_component_register(SRCS ${app_sources} REQUIRES wifi_manager ble_prov app_state_machine esp_event hal_io)
