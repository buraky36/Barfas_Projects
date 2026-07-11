#ifndef SHIFT_REGISTER_MAP_H
#define SHIFT_REGISTER_MAP_H

/*
 * Bit mapping for the dual 74HC595 Shift Registers
 */

// Shift Register 1 (LEDs 1-8)
#define SR1_LED_EN_1 (1 << 0) // QA
#define SR1_LED_EN_2 (1 << 1) // QB
#define SR1_LED_EN_3 (1 << 2) // QC
#define SR1_LED_EN_4 (1 << 3) // QD
#define SR1_LED_EN_5 (1 << 4) // QE
#define SR1_LED_EN_6 (1 << 5) // QF
#define SR1_LED_EN_7 (1 << 6) // QG
#define SR1_LED_EN_8 (1 << 7) // QH

// Shift Register 2 (LEDs 9-12, Indicators, Relay)
#define SR2_LED_EN_9 (1 << 0)     // QA
#define SR2_LED_EN_10 (1 << 1)    // QB
#define SR2_LED_EN_11 (1 << 2)    // QC
#define SR2_LED_EN_12 (1 << 3)    // QD
#define SR2_WIFI_LED_EN (1 << 4)  // QE
#define SR2_RED_LED_EN (1 << 5)   // QF
#define SR2_GREEN_LED_EN (1 << 6) // QG
#define SR2_RELAY_EN (1 << 7)     // QH

#endif // SHIFT_REGISTER_MAP_H
