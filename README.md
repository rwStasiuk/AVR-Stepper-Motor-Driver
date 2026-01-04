# AVR-Stepper-Motor-Driver
Blocking stepper motor driver for unipolar motor control on classic AVR devices

---

## Table of Contents

- [Description](#description)
  
- [Features](#features)
  
- [Firmware Package](#firmware-package)
  
- [Build / Integration](#buildintegration)
  
- [Usage Example](#useage-example)
  
- [Configuration Options](#configuration-options)
  
- [Exception Handling](#exception-handling)
  
- [Supported Devices / Hardware](#supported-deviceshardware)
  
- [Design Philosophy](#design-philosophy)
  
- [Future Versions](#future-versions)

---

## DESCRIPTION

This project provides a blocking stepper motor driver for classic AVR microcontrollers using direct GPIO control. The driver is designed to be simple, predictable, and portable across 8-bit AVR devices without relying on hardware timers, interrupts, or floating-point arithmetic.

Motor speed is controlled using fixed-point RPM scaling (RPM × 100), and stepping is performed through pre-defined coil energization patterns for full-step, half-step, and wave-step modes. All timing is generated using _delay_us() / _delay_ms(), allowing the driver to operate independently of MCU timer resources.

The driver uses an opaque Stepper structure and a separate configuration structure to enforce correct initialization and prevent accidental modification of internal state. GPIO updates are performed in a shared-port-safe manner, ensuring that unrelated pins on the same port are not disturbed.

This driver is intended for low to moderate speed motion control for simple automation tasks where blocking behavior is acceptable. It is not intended for high-speed motion control, real-time multitasking systems, or applications requiring acceleration profiles. For those use cases, a timer- or interrupt-driven design is recommended.

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

## BUILD/INTEGRATION

---

## USEAGE EXAMPLE

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

## CONFIGURATION OPTIONS

Many of the driver's public configuration options are denoted by enumerators and macros. Some of these options are discussed breifly here, but it is recommended to read through the header file for a detailed breakdown.

- **Port:** Port is an attribute of the configuration structure. There are several pre-defined macros which can be used for the port assignment, including STEPPER_PORT_A, STEPPER_PORT_B, STEPPER_PORT_C and STEPPER_PORT_B. These macros encapsulate the data direction and port registers to ensure that viable register pairs are used. Other stepper port macros can be easily defined as long as the AVR device uses memory-mapped DDRx / PORTx GPIO registers.

- **Full Steps Per Revolution:** The number of full steps the motor makes per revolution is an attribute of the configuration strucutre. Note that this value refers to the number of output steps seen at the motor shaft (i.e. gear ratio must be applied). This number can be derived from a stepper motor's datasheet as 360^o^ / internal step angle * gear ratio.

- **RPM:** The driver interprets motor speeds in revolutions per minute and this parameter can be assigned via setter function. A scaling factor of 100 must be applied to all entries for fixed-point arithmetic (e.g. for a speed of 12.5rpm, the user should pass 1250 to the setter). To select an appropriate motor speed, visit this [RPM visualizer](https://makermotor.com/rpm-visualizer/?srsltid=AfmBOopJrw4glwppHXRBRT7lEQf-tR5PVJqZms0T5wiTYbMpBtnEf3uF).
  
- **Direction:** This parameter can be assigned via setter function. Motor direction is denoted by the CW (clockwise) and CCW (counter-clockwise) enumerators. The driver interprets direction looking down at the motor shaft.
  
- **Stepping Mode:** This parameter can be assigned via setter function. The driver supports three stepping modes, denoted by the FULLSTEP, HALFSTEP and WAVESTEP enumerators.

---

## EXCEPTION HANDLING

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

---

## FUTURE VERSIONS







