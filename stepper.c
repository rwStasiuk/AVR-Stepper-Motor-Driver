/* 
 * File:   stepper.c
 * Author: reids
 *
 * Created on December 7, 2025, 6:42 PM
 */


/*===============================================================
 * INCLUSIONS
 *===============================================================*/
#include <avr/io.h>     // hardware registers
#include <util/delay.h> // _delay_ms(), _delay_us()
#include <stdbool.h>    // bool type
#include <stdint.h>     // uint8_t, uint16_t
#include "stepper.h"    // header file


/*===============================================================
 * MACROS
 *===============================================================*/
#define PINS_PER_PORT 8



/*===============================================================
 * INTERNAL TYPES (ENUMS, STRUCTS)
 *===============================================================*/

typedef struct {
    uint8_t multiplier;             // full-step ? 1, half-step ? 2, etc
    uint8_t patternCount;           // how many patterns in the stepping table
    const uint8_t *pattern;         // pointer to the pattern array
} StepModeInfo;


typedef struct {
    volatile uint8_t *ddr;          // pointer to the data direction register (i.e. &DDRB) 
    volatile uint8_t *port;         // pointer to the port register (i.e. &PORTB) 
    
    uint8_t pin1, pin2, pin3, pin4; // pin numbering
    
    uint16_t fullStepsPerRev;       // number of full mechanical steps per output revolution
    uint16_t effStepsPerRev;        // the effective number of steps per output revolution based on step mode
    
    uint16_t rpm_x100;              // scaled motor speed in RPM (i.e. 1550 -> 15.5rpm)
    uint32_t stepDelay_us;          // delay amount in microseconds
    
    StepMode stepMode;              // full-step, half-step or wave-step enum
    StepDir direction;              // clockwise or counterclockwise enum
    uint8_t stepIndex;              // to keep track of step pattern index
    
    bool initialized;               // true only after the the motor has been correctly initialized
} Stepper;



/*===============================================================
 * CONSTANTS
 *===============================================================*/

// coil energization patterns
static const uint8_t fullStepPattern[] = {0b0011, 0b0110, 0b1100, 0b1001};
static const uint8_t halfStepPattern[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};
static const uint8_t waveStepPattern[] = {0b0001, 0b0010, 0b0100, 0b1000};


// lookup table for step modes
static const StepModeInfo stepModeTable[] = {
    [FULLSTEP] = {.multiplier = 1, .patternCount = 4, .pattern = fullStepPattern},
    [HALFSTEP] = {.multiplier = 2, .patternCount = 8, .pattern = halfStepPattern},
    [WAVESTEP] = {.multiplier = 1, .patternCount = 4, .pattern = waveStepPattern}
};



/*===============================================================
 * STATIC HELPER FUNCTIONS
 *===============================================================*/

// calculates the delay between steps when step mode or rpm is updated
static void stepper_set_delay(Stepper *stepper){
    if (stepper->rpm_x100 == 0 || stepper->effStepsPerRev == 0) {
        stepper->stepDelay_us = 0;
        return;
    }
    /*
     * rev/min = 1/100 * rev_x100
     * steps/second = rev/min * 1min/60s * steps/rev
     * seconds/step = 1/(steps/second)
     * microseconds/step = step delay = 1000000 * seconds/step
     */
    uint32_t delay_us = (uint32_t)(6000000000ULL / ((uint64_t)stepper->rpm_x100 * stepper->effStepsPerRev));
    
    if (delay_us < STEPPER_MIN_STEP_DELAY_US){
        delay_us = STEPPER_MIN_STEP_DELAY_US;
    }
    if (delay_us > STEPPER_MAX_STEP_DELAY_US){
        delay_us = STEPPER_MAX_STEP_DELAY_US;
    }
    stepper->stepDelay_us = delay_us;
}


// realizes the step delay without passing variable or large values into _delay_us()
static inline void stepper_delay_us(uint32_t us)
{
    while (us >= 1000) {
        _delay_ms(1);
        us -= 1000;
    }
    while (us--) {
        _delay_us(1);
    }
}

/*===============================================================
 * PUBLIC API FUNCTIONS
 *===============================================================*/

// initializer for Stepper instance (must be called before any other API function)
StepStatus stepper_init(Stepper *stepper, const StepperConfig *config){
        
    // check if motor structure has been instantiated
    if (!stepper){
        return STEPPER_ERR_NULL_MOTOR_INSTANCE;
    }
    
    // check if the configuration structure has been instantiated
    if (!config){
        return STEPPER_ERR_NULL_CONFIG_INSTANCE;
    }
    
    // check if register pointers are null
    if(!config->port.ddr || !config->port.port){
        return STEPPER_ERR_NULL_REGISTER_POINTER;
    }
    
    // check if pin assignments are valid
    uint8_t maxPinNumber = PINS_PER_PORT - 1;
    if (config->pin1 > maxPinNumber || config->pin2 > maxPinNumber || config->pin3 > maxPinNumber || config->pin4 > maxPinNumber){
        return STEPPER_ERR_INVALID_PIN_ASSIGNMENT;
    }
    
    if (config->fullStepsPerRev == 0) {
        return STEPPER_ERR_INVALID_FULLSTEPS_PER_REV;
    }

    // transferring configuration data to the opaque stepper structure
    stepper->ddr = config->port.ddr;
    stepper->port = config->port.port;
    stepper->pin1 = config->pin1;
    stepper->pin2 = config->pin2;
    stepper->pin3 = config->pin3;
    stepper->pin4 = config->pin4;
    stepper->fullStepsPerRev = config->fullStepsPerRev;
    
    // hardware setup
    *(stepper->ddr) |= ((1 << stepper->pin1) | (1 << stepper->pin2) | (1 << stepper->pin3) | (1 << stepper->pin4));
    *(stepper->port) &= ~((1 << stepper->pin1) | (1 << stepper->pin2) | (1 << stepper->pin3) | (1 << stepper->pin4));
    
    // default setup
    stepper->stepMode = WAVESTEP;
    stepper->direction = CW;
    stepper->rpm_x100 = 0;
    stepper->stepDelay_us = 0;
    stepper->stepIndex = 0;
    stepper->effStepsPerRev = stepper->fullStepsPerRev *(stepModeTable[stepper->stepMode].multiplier);
    stepper->initialized = true;
    return STEPPER_OK;
}


// setter for the step mode of a Stepper instance
StepStatus stepper_set_mode(Stepper *stepper, StepMode mode){
    
    // check whether the stepper argument points to an instantiated Stepper structure
    if (!stepper){
        return STEPPER_ERR_NULL_MOTOR_INSTANCE;
    }
    
    // check whether the Stepper instance has been initialized
    if(!stepper->initialized){
        return STEPPER_ERR_NOT_INITIALIZED;
    }
    
    // check that the mode argument is a valid Stepmode
    if (mode < FULLSTEP || mode > WAVESTEP){
        return STEPPER_ERR_INVALID_STEP_MODE; 
    }
    
    stepper->stepMode = mode;
    
    // update the necessary operating parameters of the Stepper instance
    stepper->effStepsPerRev = stepper->fullStepsPerRev *(stepModeTable[stepper->stepMode].multiplier);
    stepper->stepIndex = 0;
    stepper_set_delay(stepper);
  
    return STEPPER_OK;
}


// setter for the step direction of the Stepper instance
StepStatus stepper_set_direction(Stepper *stepper, StepDir dir){
    
    // check whether the stepper argument points to an instantiated Stepper structure
    if (!stepper){
        return STEPPER_ERR_NULL_MOTOR_INSTANCE;
    }
    
    // check whether the Stepper instance has been initialized
    if(!stepper->initialized){
        return STEPPER_ERR_NOT_INITIALIZED;
    }
    
    // check that the dir argument is a valid StepDirection
    if (dir < CW || dir > CCW){
        return STEPPER_ERR_INVALID_STEP_DIRECTION; 
    }
    
    stepper->direction = dir;
    
    return STEPPER_OK;
}


// setter for the speed of the Stepper instance
// argument should be 100x the desired rpm
StepStatus stepper_set_rpm_x100(Stepper *stepper, uint16_t rx100){
    
    // check whether the stepper argument points to an instantiated Stepper structure
    if(!stepper){
        return STEPPER_ERR_NULL_MOTOR_INSTANCE;
    }
    
    // check whether the Stepper instance has been initialized
    if(!stepper->initialized){
        return STEPPER_ERR_NOT_INITIALIZED;
    }
    
    stepper->rpm_x100 = rx100;
    
    // update the necessary operating parameters of the Stepper instance
    stepper_set_delay(stepper);
    
    return STEPPER_OK;
}


// blocking step engine 
StepStatus stepper_step(Stepper *stepper, uint16_t stepCount){
    
    // check whether the stepper argument points to an instantiated Stepper structure
    if(!stepper){
        return STEPPER_ERR_NULL_MOTOR_INSTANCE;
    }
    
    // check whether the Stepper instance has been instantiated
    if(!stepper->initialized){
        return STEPPER_ERR_NOT_INITIALIZED;
    }
    
    // verify that the Stepper instance has a non-zero speed
    if(stepper->rpm_x100 == 0){
        return STEPPER_ERR_ZERO_RPM;
    }
    
    // pre-calculate the mask for our 4 pins
    const uint8_t pinMask = (1 << stepper->pin1) | (1 << stepper->pin2) | (1 << stepper->pin3) | (1 << stepper->pin4);
    
    for(uint16_t i = 0; i < stepCount; i++){
        uint8_t index = stepper->stepIndex; // retrieve the current index in the array of coil energization patterns

        // pinState is the 4-bit binary coil energization pattern at the current index
        uint8_t pinState = (stepModeTable[stepper->stepMode].pattern)[index];

        // calculate the new bit pattern for our pins
        uint8_t nextBits = 0;
        if (pinState & 0b0001) nextBits |= (1 << stepper->pin1);
        if (pinState & 0b0010) nextBits |= (1 << stepper->pin2);
        if (pinState & 0b0100) nextBits |= (1 << stepper->pin3);
        if (pinState & 0b1000) nextBits |= (1 << stepper->pin4);

        // read current PORT, clear only our 4 pins, and OR in the nextBits
        uint8_t portBuffer = *(stepper->port);
        portBuffer &= ~pinMask;   // clear only the motor pins in the buffer
        portBuffer |= nextBits;   // set the new states in the buffer
        *(stepper->port) = portBuffer; // write the whole byte to hardware in one cycle

        // update stepIndex by incrementing forward for CW motion and incrementing in reverse for CCW motion
        if (stepper->direction == CW){
            stepper->stepIndex = (index + 1) % (stepModeTable[stepper->stepMode].patternCount);
        }
        else {
            stepper->stepIndex = (index == 0 ? stepModeTable[stepper->stepMode].patternCount - 1 : index - 1);
        }
        
        // delay stepping by the pre-calculated microsecond count to achieve the desired rpm
        // uses _delay_us() which occupies the CPU (excluding interrupts) until the function returns
        stepper_delay_us(stepper->stepDelay_us);
    }
    return STEPPER_OK;
}


// de-energize all coils and place the motor in an idle state
StepStatus stepper_idle(Stepper *stepper){
    
    // check whether the stepper argument points to an instantiated Stepper structure
    if (!stepper){
        return STEPPER_ERR_NULL_MOTOR_INSTANCE;
    }

    // check whether the Stepper instance has been initialized
    if (!stepper->initialized){
        return STEPPER_ERR_NOT_INITIALIZED;
    }

    // create a mask for the motor control pins
    const uint8_t pinMask = (1 << stepper->pin1) | (1 << stepper->pin2) | (1 << stepper->pin3) | (1 << stepper->pin4);

    // clear only the motor pins, preserving unrelated pins on the same port
    uint8_t portBuffer = *(stepper->port);
    portBuffer &= ~pinMask;
    *(stepper->port) = portBuffer;

    return STEPPER_OK;
}