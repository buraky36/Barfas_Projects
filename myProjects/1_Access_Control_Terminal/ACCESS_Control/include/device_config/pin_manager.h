#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

/*
 * Hardware Pin Definitions for Smart Access Control Terminal
 * Centralized mapping to allow easy adaptation to different MCUs.
 */

// MFRC522 (RFID) SPI Pins
#define PIN_RFID_MOSI 23
#define PIN_RFID_MISO 19
#define PIN_RFID_CLK 18
#define PIN_RFID_CS 5
#define PIN_RFID_IRQ 22
#define PIN_RFID_RST 21

// TSM12 (Touch keypad) I2C Pins
#define PIN_TSM12_SDA 25
#define PIN_TSM12_SCL 26
#define PIN_TSM12_INT 27
#define PIN_TSM12_OUT 14
#define PIN_TSM12_RST 12

// XR1300 (QR Reader) UART Pins
#define PIN_QR_TX 17
#define PIN_QR_RX 16

// 74HC595 Shift Register Pins
#define PIN_SR_CLK 32
#define PIN_SR_DATA 33
#define PIN_SR_LATCH1 25
#define PIN_SR_LATCH2 26

// Buzzer PWM Pin
#define PIN_BUZZER_PWM 13

// Wiegand (Input/Output) Pins
#define PIN_WIEGAND_D0 4
#define PIN_WIEGAND_D1 15

// RS485 Pins
#define PIN_RS485_RX 34
#define PIN_RS485_TX 35
#define PIN_RS485_DE_RE 36 // Data Enable / Receive Enable

// Digital INPUT
#define PIN_DIGITAL_IN 39 // Example input pin
#define PIN_DOOR_SENSOR 39
#define PIN_EXIT_BUTTON 36

// Hardware Interfaces Definitions
#define I2C_PORT_TSM12 I2C_NUM_0
#define UART_PORT_QR UART_NUM_1

#endif // PIN_MANAGER_H
