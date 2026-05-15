This MCU_SDK is an MCU code automatically generated according to the product functions defined on the Tuya development platform. Modifying and supplementing on this basis can quickly complete the MCU program.


Development steps:

1: It needs to be configured according to the actual situation of the product (reset wifi button and wifi status indicator processing method, whether it supports MCU upgrade, etc.), please modify this configuration in protocol.h;

2: Migrate this MCU_SDK, please check the migration steps in the protocol.c file, and complete the migration correctly. After transplantation, please complete the code of data transmission processing and data reporting part to complete all wifi functions.

Document overview:
This MCU_SDK includes 9 files:
The
(1) system.c and system.h are wifi universal protocol parsing implementation codes, no special circumstances, users do not need to modify.
The
(2) mcu_api.c and mcu_api.h, the functions that users need to actively call are in this file.
The
(3) protocol.h and protocol.c, the processing function of the data after receiving the module data, can be found in this file, and the user needs to modify and improve the related functions. The protocol.h and protocol.c files have detailed modification instructions, please read them carefully.

(4) lock_api.c and lock_api.h, the related functions of the door lock class and the module data processing function of the door lock class that the user needs to actively call are in this file.
The
(5) The wifi.h file contains all the above .h files and defines the macro definitions used in the functions in all the above files. To use this SDK related function, please #include "wifi.h".