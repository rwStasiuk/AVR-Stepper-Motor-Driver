# AVR-Stepper-Motor-Driver
Blocking stepper motor driver for unipolar motor control on classic AVR devices

---

## Table of Contents

- [Description](#description)
  
- [Features](#features)
  
- [Firmware Package](#firmware-package)
  
- [Build Integration](#buildintegration)
  
- [Usage Example](#usage-example)
  
- [Configuration Options](#public-api-usage)
  
- [Exception Handling](#exception-handling)
  
- [Supported Devices and Hardware](#supported-devices-and-hardware)
  
- [Design Philosophy](#design-philosophy)
  
- [Future Versions](#future-versions)

---

## DESCRIPTION

This project provides a blocking unipolar stepper motor driver for classic AVR microcontrollers using direct GPIO control. The driver is designed to be simple, predictable, and portable across 8-bit AVR devices without relying on hardware timers, interrupts, or floating-point arithmetic. The driver is designed to control multiple motors from the same device, such that only one instance of the driver and header files must be included in the users project.

This driver is intended for low to moderate speed motion control for simple automation tasks where blocking behavior is acceptable. It is not intended for high-speed motion control, real-time multitasking systems, or applications requiring acceleration profiles. For those use cases, a timer- or interrupt-driven design is recommended.

Motor speed is controlled using fixed-point RPM scaling (RPM × 100), and stepping is performed through pre-defined coil energization patterns for full-step, half-step, and wave-step modes. All timing is generated using _delay_us() / _delay_ms(), allowing the driver to operate independently of MCU timer resources.

The driver uses an opaque Stepper structure and a separate configuration structure to enforce correct initialization and prevent accidental modification of internal state. GPIO updates are performed in a shared-port-safe manner, ensuring that unrelated pins on the same port are not disturbed.

If you are new to stepper motors, I highly recommend this [page by Oriental Motor](https://www.orientalmotor.com/stepper-motors/technology/stepper-motor-basics.html).

---

## FEATURES

- Blocking stepper motor control (no timers or interrupts required)

- Supports full-step, half-step, and wave-step modes
  
- Fixed-point RPM control (rpm × 100, no floating point)
  
- Shared-port safe GPIO updates
  
- Configurable minimum and maximum step delay clamps
  
- Opaque driver structure with validated initialization

---

## FIRMWARE PACKAGE

1. Driver implementation (`stepper.c`)
2. Usage example (`stepper_usage.c`)
3. Public API and configuration structures (`stepper.h`)
4. Documentation (`README.md`)

---

## BUILD INTEGRATION

This driver is written in standard C and is intended to be compiled using **avr-gcc** as part of an existing AVR firmware project. No external libraries are required beyond the standard AVR headers.

### Requirements

- AVR-GCC toolchain  
- AVR libc  
- Target device supported by `<avr/io.h>` and `<util/delay.h>`

### File Integration

Add the following files to your project:

- `stepper.c` – driver implementation  
- `stepper.h` – public API and configuration structures  

Ensure that `stepper.c` is compiled and linked with the rest of your firmware.

### Clock Frequency Configuration

This driver relies on `_delay_us()` and `_delay_ms()` for timing. The CPU clock frequency must be defined correctly via `F_CPU` before including `<util/delay.h>`. The `F_CPU` macro is not defined in the `stepper.h` file and should be defined in the user's source code.

```c

#define F_CPU 16000000UL
#include <util/delay.h>

```

Incorrect `F_CPU` values will result in inaccurate step timing and motor speed.

### OPTIMIZATION

It is recommended to compile with optimizations enabled (e.g. -O1, -O2, or -Os). The delay routines and stepping logic rely on predictable instruction timing, and unoptimized builds may produce inconsistent results.

---

## USAGE EXAMPLE

```C
#include "stepper.h"
#include <avr/io.h>

int main(void) {
    
    
    /* set up ----------------------- */
    
    // define stepper configuration parameters
    StepperConfig cfg = {
        .port = STEPPER_PORT_B,
        .pin1 = 0,
        .pin2 = 1,
        .pin3 = 2,
        .pin4 = 3,
        .fullStepsPerRev = 2048
    };

    // create a motor object
    Stepper stepper = STEPPER_INIT;
    
    //initialize the motor object
    stepper_init(&stepper, &cfg);
    
    // use public setters from the header file to operate the stepper motor
    stepper_set_mode(&stepper, FULLSTEP);
    stepper_set_direction(&stepper, CW);
    stepper_set_rpm_x100(&stepper, 1250);
    
    
    /* operation ----------------------- */
        
    // to interleave other operations between steps (blocking)
    for(int i = 0; i < 200; i++){
        StepStatus status = stepper_step(&stepper, 1);
        if(status != 0){
            printf("A stepping error has occurred");
            return 1;
        }
        // other operations (fast operations only to preserve speed)
    }
    
    // same thing as above but built in (nothing can be interwoven)
    stepper_step(&stepper, 200);  
    
    return 0;
}
```

---

## PUBLIC API USAGE

### Initialization Lifecycle
There are three steps involved in the initialization of a stepper motor instance:

1. Instantiate a `StepperConfig` structure and assign all attributes:
   ```c
   StepperConfig cfg = {
     .port = STEPPER_PORT_B,
     .pin1 = 0,
     .pin2 = 1,
     .pin3 = 2,
     .pin4 = 3,
     fullStepsPerRev = 2048
   };
   ```

2. Instantiate a `Stepper` strucutre with the default initialization macro:
   ```c
   Stepper motor1 = STEPPER_INIT;
   ```

3. Call the `stepper_init` function, using pointers to the previously instantiated `StepperConfig` and `Stepper` strucutres as arguments. Note that the initialization function must be called once before any other API function. It is also best practice to validate the `StepStatus` return value from `stepper_init`, ensuring that the `STEPPER_OK` enumerator is returned.
   ```c
   uint8_t init_status = stepper_init(&stepper, &cfg);
   if (init_status != 0){
     printf("Initialization Error!");
     // handle exception
   }
   

### Motion Configuration
(stepper_set_mode, stepper_set_direction, stepper_set_rpm_x100)

### Stepping Operations
(stepper_step behavior, blocking semantics, interleaving)

### Idling and Power Control
(stepper_idle, torque release, power considerations)

### Common Usage Patterns
(single-step loops, batch stepping, safe stopping)

### Common Mistakes
(zero RPM, forgetting init, wrong F_CPU, blocking expectations)

---

## EXCEPTION HANDLING

All public driver functions return a StepStatus value indicating success or the reason for failure. This allows the application to detect configuration errors, invalid usage, or runtime issues without relying on assertions or undefined behavior. The return value should always be checked, especially during initialization and stepping operations.

### Status Codes:

The driver defines the following StepStatus values:

- `STEPPER_OK`: Operation completed successfully.

- `STEPPER_ERR_NULL_MOTOR_INSTANCE`: A `NULL` pointer was passed instead of a valid Stepper instance.

- `STEPPER_ERR_NULL_CONFIG_INSTANCE`: A `NULL` pointer was passed instead of a valid `StepperConfig` structure.

- `STEPPER_ERR_NULL_REGISTER_POINTER`: One or more GPIO register pointers in the configuration are `NULL`.

- `STEPPER_ERR_INVALID_PIN_ASSIGNMENT`: One or more configured pin numbers exceed the valid range for the selected port.

- `STEPPER_ERR_INVALID_FULLSTEPS_PER_REV`: `fullStepsPerRev` was set to zero, which would result in undefined timing behavior.

- `STEPPER_ERR_NOT_INITIALIZED`: A public API function was called before `stepper_init()` completed successfully.

- `STEPPER_ERR_INVALID_STEP_MODE`: An invalid stepping mode was passed to `stepper_set_mode()`.

- `STEPPER_ERR_INVALID_STEP_DIRECTION`: An invalid direction was passed to `stepper_set_direction()`.

- `STEPPER_ERR_ZERO_RPM`: A stepping operation was requested while the motor speed was set to zero.

### User Guidelines

- Always check the return value of `stepper_init()` before proceeding.

- Setter functions should be checked during development to catch invalid parameters early.

- `stepper_step()` should be checked in production code if step counts or runtime parameters may vary dynamically.

- The driver does not attempt to recover from errors internally. All error handling decisions are left to the application layer.

### Example

```c

StepStatus status = stepper_step(&stepper, 1);
if (status != STEPPER_OK) {
    // Handle error (log, halt, retry, etc.)
}

```

---

## SUPPORTED DEVICES/HARDWARE

Classic 8-bit AVR microcontrollers with:
  - Memory-mapped DDRx / PORTx GPIO registers
  - `<avr/io.h>` and `<util/delay.h>` support

Examples:
- ATmega328P
- ATmega168 / ATmega88
- ATmega32 / ATmega16
- ATmega644 / ATmega1284
- ATtiny85 (limited I/O)
- ATtiny2313 / ATtiny4313

## Not Supported

- AVR DA/DB series (PORTMUX / VPORT architecture)
- ARM / STM32 / ESP32
- Arduino `digitalWrite()` abstractions

---

## DESIGN PHILOSOPHY

This driver is designed to be simple, explicit, and predictable for classic 8-bit AVR microcontrollers. All behavior is blocking and synchronous, with no reliance on hardware timers, interrupts, or background execution.

The API favors determinism over abstraction. All timing, state changes, and GPIO updates occur directly and visibly in user-invoked function calls, making execution flow easy to reason about and debug.

A strict separation is maintained between configuration, internal state, and operation. Initialization is mandatory and validated, and errors are reported explicitly via return status codes rather than hidden recovery mechanisms.

The driver avoids floating-point arithmetic, dynamic memory, and framework-style abstractions. Instead, it relies on fixed-point math and direct register access to remain lightweight, portable, and AVR-native.

This project is intentionally scoped for low to moderate speed motion control where blocking behavior is acceptable. It is meant to serve as a clear and dependable reference implementation rather than a feature-complete motion control system.

---

## FUTURE VERSIONS

- Acceleration ramp functions
- Optional interrupt-driven, non-blocking step engine
- API and configuration parameters for bipolar stepper motors






