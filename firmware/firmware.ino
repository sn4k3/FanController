/*
 *  PTRTECH FAN CONTROLLER
 *  Author: Tiago Conceição
 *  Version 1.0
 *
 *                       ATtiny25/45/85
 *                      -------u-------
 *  RST - A0 - (D 5) --| 1 PB5   VCC 8 |-- +5V
 *                     |               |
 * SWITCH A3 - (D 3) --| 2 PB3   PB2 7 |-- (D 2) - A1  --> 10K Potentiometer
 *                     |               |
 * MOSFET A2 - (D 4) --| 3 PB4   PB1 6 |-- (D 1) - PWM --> FET -> Fan PWM Wire
 *                     |               |
 *              Gnd ---| 4 GND   PB0 5 |-- (D 0) - PWM --> Disabled
 *                     -----------------
 *  https://thewanderingengineer.com/2014/08/11/pin-change-interrupts-on-attiny85/
 *
 *
 */

 /********************************
 *           Includes            *
 ********************************/
 // normal delay() won't work anymore because we are changing Timer1 behavior
 // Adds delay_ms and delay_us functions
#include <util/delay.h>    // Adds delay_ms and delay_us functions

// Clock at 8mHz
#define F_CPU 8000000L  // This is used by delay.h library

#include <EEPROM.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "macros.h"

/********************************
*           Constants           *
*          Don't Touch          *
********************************/
#define PROGRAM_VERSION         1.0 // Program Version
#define EPPROM_VERSION          1   // EPPROM Version, increment when add new features the the 'UserSettings' structure
#define EPPROM_ADDRESS_SETTINGS 0   // EEPROM start read/write address


#define ANALOG_POT_MIN_VALUE    0   // Analog min possible value
#define ANALOG_POT_MAX_VALUE    928 // Analog max possible value 4.57V


/********************************
*        Pins assignment        *
********************************/
#define PIN_OUTPUT_FAN_GND      4     // Output for FET Gate that controls fan PWM pulse
#define PIN_INPUT_POT           A1    // Input  for POT signal to convert into speed
#define PIN_INPUT_SWITCH        A3    // Input  for switch to toggle fan GND FET


/********************************
*           Settings            *
********************************/
// EEPROM
#define USE_EEPROM_SETTINGS     true      // True to use EPPROM and user settings, otherwise user settings will be ignored and behave with defaults and no saves

// Fan
#define FAN_STARTUP_KICKSTART   true      // True for fan kickstart on boot for a period of time at a pre-defined speed
#define FAN_RE_ENABLE_KICKSTART true      // True for fan kickstart on fan enable by user from a disabled state for a period of time at a pre-defined speed
#define FAN_KICKSTART_DURATION  2000      // Fan kickstart duration in miliseconds
#define FAN_KICKSTART_SPEED     FAN_SPEED_MAX  // Fan kickstart speed (PWM Range Value)
#define FAN_MIN_PWM_TO_ROTATE   2        // Min fan PWM value to be able to rotate, to disable set to 0!
#define FAN_FIXED_SPEED         0        // When positive ignores potentiometer and run fan always at a fixed speed, in a PWM range, from 1 to PWM_MAX_VALUE. Switch still applies.
#define FAN_ENABLE_USER_SWITCH  true     // True to allow user switch on/fan with push button, otherwise false

// System control
#if FAN_FIXED_SPEED <= 0
    #define DELAY_MS 100    // Loop delay, repeat, 100ms
#else
    #define DELAY_MS 360000 // Loop delay, repeat, 1h
#endif

#define DEBOUNCE_MS          200 // the debounce time for switch interrupt; increase if the output flickers
#define ANALOG_SMOOTHING_AVG 0	 // Smooth analog readings with a average count of x reads, to read once or if use hardware smoothing set 0



/********************************
*             Enums             *
*          Don't Touch          *
********************************/
enum FanState : uint8_t {       // Fan power states
    FAN_STATE_OFF_BYSPEED = 0,  // Fan off by low speed
    FAN_STATE_ON,               // Fan ON
    FAN_STATE_OFF_BYSWITCH      // Fan off by user push button
};

enum PowerOnAction : uint8_t {   // Power on actions
    POWERON_ACTION_DEFAULT = 0,  // Power on and respect speed value based on user settings
    POWERON_ACTION_OFF,          // Power on with fan disabled, user must enable it manually with button
    POWERON_ACTION_USELASTSTATE  // Power on with last known state, if user had disabled fan it will power on with fan disabled, otherwise will respect and set speed just like default
};

/********************************
*            Structs            *
*          Don't Touch          *
********************************/
struct UserSettings {
    uint16_t eppromVersion;  // epprom version
    uint8_t	 powerOnAction;  // Power on behaviour
    bool	 lastFanState;   // true = Fan was on before power off. False = Fan disabled by user before power off
} userSettings;

#if defined(ANALOG_SMOOTHING_AVG) && ANALOG_SMOOTHING_AVG > 1
struct AnalogSmoothing {                     // Represents an analog smoothing calculations, one per port
    uint16_t readings[ANALOG_SMOOTHING_AVG]; // the readings from the analog input
    uint8_t  readIndex;                      // the index of the current reading
    uint16_t total;                          // the running total
    uint16_t average;                        // the average
} smoothPot;

/*
 * Function: analogSmoothingAddRead
 * ----------------------------
 *   Add a value to a current smoothing struct, calculate the total and average.
 *
 *   structVar: The struct reference to work with
 *   value: Value to add and calculate the average
 *
 *   returns: The smoothed average value
 */
uint16_t analogSmoothingAddRead(struct AnalogSmoothing*structVar, const uint16_t value)
{
    structVar->total -= structVar->readings[structVar->readIndex]; // subtract the last reading
    structVar->readings[structVar->readIndex] = value;             // read from the sensor
    structVar->total += structVar->readings[structVar->readIndex]; // add the reading to the total
    structVar->readIndex++;                                        // advance to the next position in the array

    
    if (structVar->readIndex >= ANALOG_SMOOTHING_AVG) {  // if we're at the end of the array...
        structVar->readIndex = 0;                        // ...wrap around to the beginning
    }

    structVar->average = structVar->total / ANALOG_SMOOTHING_AVG; // calculate the average

    return structVar->average;
}

#endif

/********************************
*          Variables            *
*         Don't Touch           *
********************************/
static uint16_t	pot_value;					// Current value read from potentiometer
static uint8_t	pwm_value;					// Current pwm value converted from potentiometer value
static FanState fan_state = FAN_STATE_ON;	// Current fan power state
static uint64_t last_interrupt_time = 0;	// Last interrupt time for debounce calculations

/********************************
*           Main Code           *
*           Functions           *
********************************/

/*
 * Function: readFanSpeed
 * ----------------------------
 *   Read fan speed from pot and convert into valid PWM value
 *
 *   returns: void
 */
void readFanSpeed() {
#if defined(ANALOG_SMOOTHING_AVG) && ANALOG_SMOOTHING_AVG > 1
    pot_value = analogSmoothingAddRead(&smoothPot, analogRead(PIN_INPUT_POT));
#else
    pot_value = analogRead(PIN_INPUT_POT);
#endif

    pwm_value = map(pot_value, ANALOG_POT_MIN_VALUE, ANALOG_POT_MAX_VALUE, PWM_MIN_VALUE, PWM_MAX_VALUE);
}

/*
 * Function: setFanSpeed
 * ----------------------------
 *   Set fan speed based on a PWM signal value
 *
 *   pwmVal: PWM pulse value
 *   constrain_pwm: True to constrain pwmVal to a valid range, otherwise false to ignore constrain or safe checks
 *
 *   returns: void
 */
void setFanSpeed(uint8_t pwm_val, const bool constrain_pwm = true) {
    if (constrain_pwm) {
        pwm_val = constrain(pwm_val, PWM_MIN_VALUE, PWM_MAX_VALUE);
    }

#if defined(FAN_MIN_PWM_TO_ROTATE) && FAN_MIN_PWM_TO_ROTATE > 0
    if (pwm_val < FAN_MIN_PWM_TO_ROTATE) {
        pwm_val = PWM_MIN_VALUE;
    }
#endif

    FanPwm = PWM_MAX_VALUE - pwm_val; // Invert logic, N-FET, open drain
    fan_state = pwm_val == 0 ? FAN_STATE_OFF_BYSPEED : FAN_STATE_ON;
    digitalWrite(PIN_OUTPUT_FAN_GND, fan_state);
}

/*
 * Function: enableFan
 * ----------------------------
 *   Enable fan and set speed by reading the potentiometer or a fixed speed
 *
 *   returns: void
 */
void enableFan() {
#if defined(FAN_FIXED_SPEED) && FAN_FIXED_SPEED > 0
    pwm_value = FAN_FIXED_SPEED;
#else
    readFanSpeed();
#endif

    //if (lastPwmValue == pwmValue) return; // Try to spare some useless cycles?

    setFanSpeed(pwm_value, false);
}

/*
 * Function: disableFan
 * ----------------------------
 *   Disable fan by set 0 PWM speed and cut fan GND by the Power FET
 *
 *   returns: void
 */
void disableFan() {
    setFanSpeed(FAN_SPEED_OFF, false);
    fan_state = FAN_STATE_OFF_BYSWITCH;
}

/*
 * Function: kickStartFan
 * ----------------------------
 *   Kickstart fan on a specific speed for a pre-defined time if current speed is able to rotate the fan
 *
 *   speed: PWM pulse value
 *   duration: Kickstart duration in ms before return to the current known speed
 *
 *   NOTES: Program will idle during duration time, no other action will perform in meantime.
 *   returns: void
 */
void kickStartFan(const uint8_t speed = FAN_KICKSTART_SPEED, const uint16_t duration = FAN_KICKSTART_DURATION)
{
    readFanSpeed();
    if (pwm_value > FAN_MIN_PWM_TO_ROTATE) {
        setFanSpeed(speed);
        _delay_ms(duration);
    }
}

void setup()
{
    pinMode(PIN_OUTPUT_FAN_PWM, OUTPUT);
    pinMode(PIN_OUTPUT_FAN_GND, OUTPUT);
    pinMode(PIN_INPUT_POT, INPUT);
    pinMode(PIN_INPUT_SWITCH, INPUT);

    INIT_PWM();

#if defined(USE_EEPROM_SETTINGS) && USE_EEPROM_SETTINGS
    EEPROM.get(EPPROM_ADDRESS_SETTINGS, userSettings);

    /*if(userSettings.eppromVersion == 0) 
    {
        // Create and set defaults
    }
    else if (userSettings.eppromVersion != EPPROM_VERSION)
    {
        // Different EPPROM version, handle differences and update
    }*/

    userSettings.eppromVersion = EPPROM_VERSION;

    if (digitalRead(PIN_INPUT_SWITCH) == LOW)
    {
        _delay_ms(DEBOUNCE_MS);
        while (digitalRead(PIN_INPUT_SWITCH) == LOW) { _delay_ms(DEBOUNCE_MS); }

        INCREMENT_RANGE_LOOP(userSettings.powerOnAction, 0, POWERON_ACTION_USELASTSTATE);
        EEPROM.put(EPPROM_ADDRESS_SETTINGS, userSettings);
    }

#endif

    if(userSettings.powerOnAction == POWERON_ACTION_OFF || (userSettings.powerOnAction == POWERON_ACTION_USELASTSTATE && !userSettings.lastFanState))
    {
        disableFan();
    }
    else 
    {
#if defined(FAN_STARTUP_KICKSTART) && FAN_STARTUP_KICKSTART && FAN_KICKSTART_DURATION >= 10
        // Kickstart fan on a specific speed for a pre-defined time if current speed is able to rotate the fan
        kickStartFan();
#endif
    }
    

#if defined(FAN_ENABLE_USER_SWITCH) && FAN_ENABLE_USER_SWITCH
    // Interrupts
    GIMSK |= (1 << PCIE);   // pin change interrupt enable
    PCMSK |= (1 << PCINT3); // pin change interrupt enabled for PCINT4                                
    sei();                  // ensure interrupts enabled so we can wake up again
#endif
}

void loop()
{
    // Read and set fan speed if not disabled
    if (fan_state != FAN_STATE_OFF_BYSWITCH)
    {
        enableFan();
        _delay_ms(500); // Futher delay as we don't need often checks when fan is disabled
    }

    _delay_ms(DELAY_MS);
}

#if defined(FAN_ENABLE_USER_SWITCH) && FAN_ENABLE_USER_SWITCH
// Interrupt for the fan switch on/off
ISR(PCINT0_vect) {
    if (digitalRead(PIN_INPUT_SWITCH) == HIGH) return; // Need a low signal first
    unsigned long interrupt_time = millis();
    // If interrupts come faster than x ms, assume it's a bounce and ignore
    if (interrupt_time - last_interrupt_time > DEBOUNCE_MS)
    {
        while (digitalRead(PIN_INPUT_SWITCH) == LOW) _delay_ms(DEBOUNCE_MS);

        if (fan_state == FAN_STATE_ON) {
            disableFan();
#if defined(USE_EEPROM_SETTINGS) && USE_EEPROM_SETTINGS
            userSettings.lastFanState = false;
            EEPROM.put(EPPROM_ADDRESS_SETTINGS, userSettings);
#endif
        }
        else {
#if defined(FAN_RE_ENABLE_KICKSTART) && FAN_RE_ENABLE_KICKSTART
            kickStartFan();
#endif
            enableFan();
#if defined(USE_EEPROM_SETTINGS) && USE_EEPROM_SETTINGS
            userSettings.lastFanState = true;
            EEPROM.put(EPPROM_ADDRESS_SETTINGS, userSettings);
#endif
        }
    }
    last_interrupt_time = interrupt_time;
}
#endif
