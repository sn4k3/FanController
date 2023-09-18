/*
 *  PTRTECH FAN CONTROLLER
 *  Author: Tiago Conceição
 *  MACROS / Helpers
 */

#ifndef _FW_MACROS_H
#define _FW_MACROS_H


// Increment a variable, when overflow the maximum it will set to minimum specified value
#define INCREMENT_RANGE_LOOP(var, range_min, range_max)    \
({                                                         \
    var++;                                                 \
    if ((var) > (range_max))                               \
        var = range_min;                                   \
})


#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega328P__)
#define ATmega_644_644P_1284P 

#define PIN_OUTPUT_FAN_PWM 3	// Output for FET Gate that controls fans GND
#define PWM_MAX_VALUE 160			// Maximum PWM value
#define OCRAUX OCR2A
#define OCRPWM OCR2B

// Phase Correct PWM Mode, no Prescaler
// PWM on Pin 3(PD3), Pin 11(PB3) disabled
// 8Mhz / 160 / 2 = 25Khz
#define SET_TIMERS() \
({ \
    TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM20); \
    TCCR2B = _BV(CS22); \
})

#elif defined(__AVR_ATtiny25__) ||defined(__AVR_ATtiny45__) ||defined(__AVR_ATtiny85__)
#define ATtiny_25_45_85 

#define PIN_OUTPUT_FAN_PWM 1  // Output for FET Gate that controls fans GND
#define PWM_MAX_VALUE 160     // Maximum PWM value
#define OCRAUX OCR0A
#define OCRPWM OCR0B

// Phase Correct PWM Mode, no Prescaler
// PWM on Pin 1(PB1), Pin 0(PB0) disabled
// 8Mhz / 160 / 2 = 25Khz
#define SET_TIMERS()\
({\
    TCCR0A = _BV(COM0B1) | _BV(WGM00);\
    TCCR0B = _BV(WGM02)  | _BV(CS00);\
})
#else
#error "Selected MCU not implemented yet, please implement first."
#endif

// Set timers
// TOP - DO NOT CHANGE, SETS PWM PULSE RATE
// duty cycle for PWMPin, disable fan
#define INIT_PWM()\
({\
    SET_TIMERS();\
    OCRAUX = PWM_MAX_VALUE;\
    OCRPWM = PWM_MAX_VALUE;\
})

#define PWM_MIN_VALUE 0             // Minimum PWM value
#define FAN_SPEED_OFF PWM_MIN_VALUE // Minimum fan speed / off
#define FAN_SPEED_MAX PWM_MAX_VALUE // Maximum fan speed / full on
#define FanPwm OCRPWM               // Don't touch!


#endif