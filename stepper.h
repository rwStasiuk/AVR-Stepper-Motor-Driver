/** 
 * File:   stepper.h
 * Author: Reid Stasiuk
 *
 * Created on December 7, 2025, 6:42 PM
 */

/**
 * @file stepper.h
 * @brief Blocking unipolar stepper motor driver for AVR micro-controllers.
 *
 * Typical usage:
 *   Stepper stepper = STEPPER_INIT;
 *   StepperConfig cfg = {...};
 *   stepper_init(&stepper, &cfg);
 *   stepper_set_rpm_x100(&stepper, 1250);
 *   stepper_set_mode(&stepper, HALFSTEP);
 *   stepper_set_direction(&stepper, CCW);
 *   stepper_step(&stepper, 200);
 */

#ifndef STEPPER_H
#define	STEPPER_H



/*===============================================================
 * INCLUDES
 *===============================================================*/

#include <stdbool.h>    // bool type
#include <stdint.h>     // uint8_t, uint16_t

/*
 * NOTE:
 * F_CPU must be defined by the application or compiler flags
 * BEFORE including <util/delay.h>.
 * It should NOT be defined inside this driver header.
 */



/*===============================================================
 * COMPILE-TIME CONFIGURATION
 *===============================================================*/

/**
 * Default step delay clamps (user may override by redefining these macros).
 *
 * These limits are software timing safeguards, not motor capability guarantees.
 *
 * Example theoretical maximum speed calculation:
 *
 * Motor: 28BYJ-48 (5V)
 * Full steps per revolution: 2048
 * Minimum step delay: 50 us
 *
 * Time per revolution:
 *   2048 steps × 50 us = 102,400 us = 0.1024 s
 *
 * Theoretical RPM:
 *   RPM = 60 s / 0.1024 s ? 585.94 RPM
 *
 * Note:
 * - This value exceeds the mechanical limits of the motor.
 * - Actual achievable RPM is much lower due to torque, inertia,
 *   and supply voltage constraints.
 *
 * Using the maximum step delay (1 second), the minimum speed in
 * full-step mode is approximately 0.03 RPM.
 *
 * In half-step mode, the effective steps per revolution double,
 * resulting in approximately half the RPM for the same step delay.
 */
#ifndef STEPPER_MIN_STEP_DELAY_US
#define STEPPER_MIN_STEP_DELAY_US 50
#endif

#ifndef STEPPER_MAX_STEP_DELAY_US
#define STEPPER_MAX_STEP_DELAY_US 1000000UL
#endif



/*===============================================================
 * PUBLIC TYPES
 *===============================================================*/

/**
 * Error-catching enums referred to as the "StepStatus" type. When the driver catches an issue,
 * it will return a non-zero enum, which can be used in the main program to decide what to do. 
 * For example:
 * 
 * StepStatus status = stepper_step(&stepper, 200);
 * if (status != STEPPER_OK) {
 * // user decides what to do
 * }
 * 
 * All public API functions return a StepStatus enum.
 */
typedef enum{
    STEPPER_OK = 0,
    STEPPER_ERR_NULL_MOTOR_INSTANCE,
    STEPPER_ERR_NULL_CONFIG_INSTANCE,
    STEPPER_ERR_NULL_REGISTER_POINTER,
    STEPPER_ERR_INVALID_PIN_ASSIGNMENT,
    STEPPER_ERR_INVALID_FULLSTEPS_PER_REV,
    STEPPER_ERR_INVALID_STEP_MODE,
    STEPPER_ERR_INVALID_STEP_DIRECTION,
    STEPPER_ERR_NOT_INITIALIZED,
    STEPPER_ERR_ZERO_RPM
} StepStatus;


// step mode enums (user can provide a named step mode argument to API functions)
typedef enum{
    FULLSTEP,
    HALFSTEP, 
    WAVESTEP
}StepMode;


// step direction enums (user can provide a named direction argument to API functions)
typedef enum{
    CW, 
    CCW
}StepDir;



/*===============================================================
 * GPIO PORT ABSTRACTION
 *===============================================================*/

// GPIO port abstraction (classic AVR only)
typedef struct {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
} StepperPort;


// port descriptor macros to be used in the instantiation of a StepperConfig structure
#ifdef DDRA
#define STEPPER_PORT_A ((StepperPort){ &DDRA, &PORTA })
#endif

#ifdef DDRB
#define STEPPER_PORT_B ((StepperPort){ &DDRB, &PORTB })
#endif

#ifdef DDRC
#define STEPPER_PORT_C ((StepperPort){ &DDRC, &PORTC })
#endif

#ifdef DDRD
#define STEPPER_PORT_D ((StepperPort){ &DDRD, &PORTD })
#endif



/*===============================================================
 * CONFIGURATION STRUCTURES
 *===============================================================*/

/**
 * Stepper configuration structure. 
 * 
 * @brief Register definitions, pin definitions and data sheet specs for the stepper motor
 * 
 * The StepperConfig structure allows the Stepper structure to be excluded from the public API,
 * so that the internal attributes of a Stepper instance cannot be accessed at runtime. A 
 * StepperConfig struct must be used to initialize a Stepper structure by passing both structures
 * to the stepper_init function (see below).
 * 
 * @param port GPIO port descriptor identifying the AVR I/O port to which
 *             the stepper driver control pins are connected.
 *
 *             This field must be initialized using one of the predefined
 *             STEPPER_PORT_x macros (e.g. STEPPER_PORT_B).
 *
 *             The descriptor internally contains the associated DDR and
 *             PORT register addresses and guarantees a valid register pair.
 * 
 * @param pin1 The integer pin number connected to in1 on the motor driver.
 * @param pin2 The integer pin number connected to in2 on the motor driver.
 * @param pin3 The integer pin number connected to in3 on the motor driver.
 * @param pin4 The integer pin number connected to in4 on the motor driver.
 * @param fullStepsPerRev The number of full steps of the motor shaft for one full revolution.
 * 
 * @note A StepperConfig structure is required to initialize a Stepper structure.
 * @note fullStepsPerRev = (360deg / internal full step angle) * gear ratio.
 *       e.g. For a 28BYJ-48 stepper motor: fullStepsPerRev = (360/11.25deg)*64 = 2048.
 */
typedef struct {
    StepperPort port;
    uint8_t pin1, pin2, pin3, pin4;
    uint16_t fullStepsPerRev;
} StepperConfig;



/*===============================================================
 * OPAQUE STRUCTURE DECLARATION
 *===============================================================*/

// opaque Stepper structure declaration
typedef struct Stepper Stepper;

// default all attributes of a Stepper instance to 0/False
#define STEPPER_INIT {0}



/*===============================================================
 * PUBLIC API
 *===============================================================*/

/**
 * @brief Initialize a stepper motor instance.
 *
 * Validates configuration parameters and transfers data from a StepperConfig instance into
 * a Stepper instance to maintain opaqueness of the Stepper structure. Configures data direction
 * and port registers. Assigns default values to Stepper.
 *
 * @param stepper Pointer to a Stepper instance.
 * @param config  Pointer to a StepperConfig structure.
 *
 * @retval STEPPER_OK Initialization successful.
 * @retval STEPPER_ERR_NULL_MOTOR_INSTANCE Stepper pointer is NULL.
 * @retval STEPPER_ERR_NULL_CONFIG_INSTANCE Config pointer is NULL.
 * @retval STEPPER_ERR_NULL_REGISTER_POINTER The data direction or port register pointer is NULL.
 * @retval STEPPER_ERR_INVALID_PIN_ASSIGNMENT Pin number out of range.
 * @retval STEPPER_ERR_INVALID_FULLSTEPS_PER_REV Zero full steps per revolution.
 *
 * @note This function must be called before any other API function.
 */
StepStatus stepper_init(Stepper *stepper, const StepperConfig *cfg);

/**
 * @brief Set step mode to full-step, half-step or wave-step.
 *
 * Validates user inputted step mode (enum) and sets the "StepMode" attribute in the Stepper instance.
 * Updates other internal operating parameters accordingly.
 *
 * @param stepper Pointer to a Stepper instance.
 * @param mode Step mode enum (FULLSTEP, HALFSTEP, WAVESTEP).
 *
 * @retval STEPPER_OK The mode was set successfully.
 * @retval STEPPER_ERR_NULL_MOTOR_INSTANCE Stepper pointer is NULL.
 * @retval STEPPER_ERR_NOT_INITIALIZED stepper_init has not been called for the Stepper argument.
 * @retval STEPPER_ERR_INVALID_STEP_MODE Invalid motor step angle.
 */
StepStatus stepper_set_mode(Stepper *stepper, StepMode mode);

/**
 * @brief Set motor to rotate clockwise or counterclockwise.
 *
 * Validates the user inputted direction (enum) and sets the "direction" attribute in the Stepper instance.
 *
 * @param stepper Pointer to a Stepper instance.
 * @param mode  Direction enum (CW or CCW).
 *
 * @retval STEPPER_OK The direction was set successfully.
 * @retval STEPPER_ERR_NULL_MOTOR_INSTANCE Stepper pointer is NULL.
 * @retval STEPPER_ERR_NOT_INITIALIZED stepper_init has not been called for the Stepper argument.
 * @retval STEPPER_ERR_INVALID_STEP_DIRECTION The "dir" argument is invalid.
 */
StepStatus stepper_set_direction(Stepper *stepper, StepDir dir);

/**
 * @brief Set the rpm at which the motor should rotate using a 100x factor.
 *
 * Validates the user inputted rpm (float) and sets the "rpm" attribute in the Stepper instance
 * Updates other internal operating parameters accordingly.
 * 
 * @param stepper Pointer to a Stepper instance.
 * @param rpm_x100  Stepper motor speed in RPM with a 100x scaling factor.
 *        (i.e. for an rpm of 12.5, pass 1250).
 *
 * @retval STEPPER_OK The motor speed was set successfully.
 * @retval STEPPER_ERR_NULL_MOTOR_INSTANCE Stepper pointer is NULL.
 * @retval STEPPER_ERR_NOT_INITIALIZED stepper_init has not been called for the Stepper argument.
 * 
 * @note The 100x scaling factor is used for fixed-point arithmetic instead of expensive floats.
 */
StepStatus stepper_set_rpm_x100(Stepper *stepper, uint16_t rpm_x100);

/**
 * @brief Step the motor the specified number of times.
 *
 * Validates that the motor instance exists and has been initialized. Validates that the user inputted
 * a valid integer number of steps and that the motor speed is non-zero. Updates the binary pattern at
 * the four pins connected to the motor based on the predefined coil energization sequences and the 
 * configuration of the Stepper instance.
 *
 * @param stepper Pointer to a Stepper instance.
 * @param stepCount the number of steps to take.
 *
 * @retval STEPPER_OK The motor was stepped successfully.
 * @retval STEPPER_ERR_NULL_MOTOR_INSTANCE Stepper pointer is NULL.
 * @retval STEPPER_ERR_NOT_INITIALIZED stepper_init has not been called for the Stepper argument.
 * @retval STEPPER_ERR_ZERO_RPM The motor speed is set to zero. Stepping cannot occur.
 *
 * @note Motor speed must be set before calling. Step mode and direction have non-NULL default values
 * @note This function blocks normal program flow (excluding interrupts) until all steps are completed.
 */
StepStatus stepper_step(Stepper *stepper, uint16_t stepCount);

/**
 * @brief De-energize all stepper motor coils and place the driver in an idle state (holds cogging torque).
 *
 * This function clears all GPIO outputs associated with the stepper motor, reducing power consumption 
 * and heat dissipation. The internal state of the Stepper instance (step mode, direction, step index,
 * and speed settings) is preserved. Subsequent calls to stepper_step() will resume motion from the current
 * logical step index once the motor is driven again.
 *
 * @param stepper Pointer to a Stepper instance.
 *
 * @retval STEPPER_OK The motor was successfully placed into an idle state.
 * @retval STEPPER_ERR_NULL_MOTOR_INSTANCE Stepper pointer is NULL.
 * @retval STEPPER_ERR_NOT_INITIALIZED stepper_init has not been called.
 *
 * @note This function does not block.
 */
StepStatus stepper_idle(Stepper *stepper);



#endif	/* STEPPER_H */

