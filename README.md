# AVR-Stepper-Motor-Driver
Blocking stepper motor driver for unipolar motor control on classic AVR devices.

---

## TABLE OF CONTENTS

- [Description](#description)
- [Features](#features)
- [Firmware Package](#firmware-package)
- [Project Integration](#project-integration)
- [Usage Example](#usage-example)
- [API Use](#api-use)
- [Exception Handling](#exception-handling)
- [Supported Devices and Hardware](#supported-devices-and-hardware)
- [Future Versions](#future-versions)

---

## DESCRIPTION

This project provides a blocking unipolar stepper motor driver for classic AVR microcontrollers using direct GPIO control. The driver is designed to be simple, predictable, and portable across 8-bit AVR devices without relying on hardware timers, interrupts, or floating-point arithmetic. This driver assumes a unipolar stepper motor controlled via a transistor array or ULN2003-style driver. GPIO pins must not drive coils directly.

This driver is intended for low to moderate speed motion control for simple automation tasks where blocking behavior is acceptable. It is not intended for high-speed motion control or real-time multitasking systems. For those use cases, a timer- or interrupt-driven design is recommended.

Motor speed is controlled using fixed-point RPM scaling (RPM × 100), and stepping is performed through pre-defined coil energization patterns for full-step, half-step, and wave-step modes. All timing is generated using `_delay_us()` / `_delay_ms()`, allowing the driver to operate independently of MCU timer resources.

The driver uses an opaque Stepper structure and a separate configuration structure to enforce correct initialization and prevent accidental modification of internal state. GPIO updates are performed in a shared-port-safe manner, ensuring that unrelated pins on the same port are not disturbed.

Multiple motors may be controlled sequentially by instantiating multiple Stepper structures. Concurrent motion is not supported due to the blocking design.

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

## PROJECT INTEGRATION

This driver is written in standard C and is intended to be compiled using **avr-gcc** as part of an existing AVR firmware project. No external libraries are required beyond the standard AVR headers.

### Requirements

- AVR-GCC toolchain  
- AVR libc  
- Target device supported by `<avr/io.h>` and `<util/delay.h>`

### File Integration

Add the following files to your project:

- `stepper.c` – driver implementation  
- `stepper.h` – public API and configuration structures  

> Ensure that `stepper.c` is compiled and linked with the rest of your firmware.

### Clock Frequency Configuration

This driver relies on `_delay_us()` and `_delay_ms()` for timing. The CPU clock frequency must be defined correctly via `F_CPU` before including `<util/delay.h>`. The `F_CPU` macro is not defined in the `stepper.h` file and should be defined in the user's source code.

```c
#define F_CPU 16000000UL
#include <util/delay.h>
```

> Incorrect `F_CPU` values will result in inaccurate step timing and motor speed.

### OPTIMIZATION

It is recommended to compile with optimizations enabled (e.g. -O1, -O2, or -Os). The delay routines and stepping logic rely on predictable instruction timing, and un-optimized builds may produce inconsistent results.

---

## USAGE EXAMPLE

```c
#define F_CPU 8000000UL
#include <avr/io.h>
#include "stepper.h"

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

    // create a motor object using the default initializer macro
    Stepper motor1 = STEPPER_INIT;
    
    //initialize the motor object
    stepper_init(&motor1, &cfg);
    
    // use public setters from the header file to operate the stepper motor
    stepper_set_mode(&motor1, FULLSTEP);
    stepper_set_direction(&motor1, CW);
    stepper_set_rpm_x100(&motor1, 1257);
    
    
    /* operation ----------------------- */
        
    // to interleave other operations between steps (blocking)
    for(int i = 0; i < 200; i++){
        StepStatus status = stepper_step(&motor1, 1);
        if(status != 0){
            // handle the exception (log, halt, retry, etc.)
            return 1;
        }
        // other operations (fast operations only to preserve speed)
    }
    
    // same thing as above but built in (nothing can be interwoven)
    stepper_step(&motor1, 200);  
    
    return 0;
}
```

---

## API USE

All API functions take as an argument a pointer to a Stepper structure (`&exampleMotor`).

### Initialization Lifecycle
There are three steps involved in the initialization of a stepper motor instance:

1. Instantiate a `StepperConfig` structure and assign all attributes. Note that the predefined port macros are denoted as:
   
   - `STEPPER_PORT_A`
   - `STEPPER_PORT_B`
   - `STEPPER_PORT_C`
   - `STEPPER_PORT_D`
   
   These macros refer to the common DDRx and PORTx GPIO register pairs found on classic AVR devices.
   
   ```c
   StepperConfig cfg = {
     .port = STEPPER_PORT_B,
     .pin1 = 0,
     .pin2 = 1,
     .pin3 = 2,
     .pin4 = 3,
     .fullStepsPerRev = 2048
   };
   ```

2. Instantiate a `Stepper` structure with the default initialization macro, which sets all structure parameters to 0 or NULL
   ```c
   Stepper motor1 = STEPPER_INIT;
   ```

3. Call the `stepper_init()` function, using pointers to the previously instantiated `StepperConfig` and `Stepper` structures as arguments. Note that the initialization function must be called once before any other API function. It is also recommended to validate the `StepStatus` enumerator which is returned by `stepper_init()`
   ```c
   uint8_t init_status = stepper_init(&motor1, &cfg);
   if (init_status != 0){
     // handle exception
   }
   ```
   
   After initialization:
   - StepMode: `WAVESTEP`
   - Direction: `CW`
   - Speed: 0 rpm
   - Coils: Idle (pins driven low)
   
### Motion Configuration
The user is provided with three motion control parameters, which are adjustable at runtime using the API setter functions. Some of these parameters are defined using enumerators, as detailed below.

**Step Mode:** Set using `stepper_set_mode()`
- Full stepping (`FULLSTEP`)
- Half stepping (`HALFSTEP`)
- Wave stepping (`WAVESTEP`)

```c
stepper_set_mode(&motor1, HALFSTEP);
```

**Direction:** Set using `stepper_set_direction()`
- Clockwise (`CW`)
- Counter-clockwise (`CCW`)

```c
stepper_set_direction(&motor1, CCW);
```

**Speed:** Set using `stepper_set_rpm_x100()`
- The user should pass their desired motor speed in RPM, multiplied by 100 (i.e. for a speed of 12.57rpm, pass 1257 to the setter function).
- Speed control is implemented by changing the length of the delay between coil energization patterns.
- Step delays are clamped between 50 and 1,000,000 microseconds, which means that motor speed is theoretically clamped between 0.03rpm and 585.94rpm by the driver. These values are purely theoretical and the achievable motor speed may vary based on the motor specifications. The `STEPPER_MIN_STEP_DELAY_US` and `STEPPER_MAX_STEP_DELAY_US` clamps are included in the API so that advanced users may adjust them as needed by redefining the macros in the application layer.

```c
stepper_set_rpm_x100(&motor1, 1257);
```

### Stepping Operations
Stepping is performed using the `stepper_step()` function, which advances the motor by a specified number of steps, based on the current configuration.

```c
stepper_step(&motor1, 200);
```

`stepper_step()` is a blocking function. Once called, it will not return until all requested steps have been completed. While interrupts are still serviced, no foreground code executes during stepping. Long step counts or low RPM values will therefore stall the main execution flow. 

Internally, the function:
- Applies the appropriate coil energization pattern
- Updates GPIO pins atomically
- Waits for the configured delay using `_delay_us()` / `_delay_ms()`
- Repeats until the requested step count is reached

To interleave other operations between steps, the user may call `stepper_step()` with a step count of 1 inside a loop. This allows limited foreground work to occur between steps, but all operations must complete within the configured step delay to avoid distorting the effective motor speed.

```c
for (uint16_t i = 0; i < 200; i++) {
    stepper_step(&motor1, 1);
    // other fast operations
}
```

### Idling and Power Control
After a stepping operation completes, the motor remains energized, holding its last position with full holding torque. In applications where holding torque is not required, the motor can be de-energized using `stepper_idle()`.

```c
stepper_idle(&motor1);
```

Calling `stepper_idle()`:
- Clears all motor control pins
- Removes holding torque (only residual cogging torque remains)
- Reduces power consumption and coil heating
- Preserves internal state (step index, mode, direction, speed)

The motor can resume operation at any time by calling `stepper_step()` again. No reinitialization is required.

### Common Mistakes
The following issues account for most runtime errors and unexpected behavior:

- Forgetting to call `stepper_init()`: All public API functions require a successfully initialized Stepper instance. Calling any function before initialization will result in `STEPPER_ERR_NOT_INITIALIZED`.
- Attempting to step with zero RPM: If motor speed is set to zero, `stepper_step()` will return `STEPPER_ERR_ZERO_RPM`. Speed must be configured before stepping.
- Incorrect `F_CPU` definition: `_delay_us()` timing depends entirely on an accurate `F_CPU` value. An incorrect clock definition will result in incorrect motor speed and timing behavior.
- Expecting non-blocking behavior: `stepper_step()` halts foreground execution until all steps complete. This driver is not suitable for multitasking or real-time scheduling without external interrupt-driven logic.
- Assuming mechanical limits match software limits: Theoretical RPM limits derived from delay clamps do not reflect real motor capabilities. Always consult the motor datasheet and validate behavior experimentally.

It is recommended to read the commenting in the header file for further details.
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

## SUPPORTED DEVICES AND HARDWARE

Classic 8-bit AVR microcontrollers with:
  - Memory-mapped DDRx / PORTx GPIO registers
  - `<avr/io.h>` and `<util/delay.h>` support

Examples:
- ATmega328P
- ATmega168 / ATmega88
- ATmega32 / ATmega16
- ATmega644 / ATmega1284
- ATtiny85
- ATtiny2313 / ATtiny4313

## Not Supported

- AVR DA/DB series (PORTMUX / VPORT architecture)
- ARM / STM32 / ESP32
- Arduino `digitalWrite()` abstractions

---

## FUTURE VERSIONS

The following features are intentionally excluded from the current driver to preserve simplicity, determinism, and blocking behavior. They may be explored as separate modules or future variants.

### Motion Profiling
- Acceleration and deceleration ramps (trapezoidal or S-curve)

> This feature requires dynamic timing updates and is better suited to a timer-driven or ISR-based design.

### Hardware Support
- Bipolar stepper motor support (H-bridge control)

> This feature would introduce different coil energization patterns and electrical assumptions.

### Execution Model
- Optional interrupt-driven, non-blocking step engine

> This feature fundamentally changes control flow and is intentionally out of scope for this blocking driver.








