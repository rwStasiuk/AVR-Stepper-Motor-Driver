
#define F_CPU 8000000UL
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
    Stepper motor1 = STEPPER_INIT;
    
    //initialize the motor object
    StepStatus s = stepper_init(&motor1, &cfg);
    if (s != 0){
        // handle exception (log, halt, retry, etc.)
    }
    
    // use public setters from the header file to operate the stepper motor
    stepper_set_mode(&motor1, FULLSTEP);
    stepper_set_direction(&motor1, CW);
    stepper_set_rpm_x100(&motor1, 1250);
    
    
    /* operation ----------------------- */
        
    // to interleave other operations between steps (blocking)
    for(int i = 0; i < 200; i++){
        StepStatus status = stepper_step(&motor1, 1);
        if(status != 0){
            // handle exception (log, halt, retry, etc.))
        }
        // other operations (fast operations only to preserve speed)
    }
    
    // same thing as above but built in (nothing can be interwoven)
    stepper_step(&motor1, 200);  
    
    return 0;
}

