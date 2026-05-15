#ifndef APSERVER_H
#define APSERVER_H

#include "Config.h"

// Initializes the server logic and stores reference to config
void ap_server_init(DeviceConfig *config);

// Starts the AP and Web Server
void ap_server_begin(void);

// Must be called in loop
void ap_server_handle_client(void);

#endif
