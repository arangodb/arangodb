/*
 * david austin
 * http://www.embedded.com/design/mcus-processors-and-socs/4006438/Generate-stepper-motor-speed-profiles-in-real-time
 * DECEMBER 30, 2004
 *
 * Demo program for stepper motor control with linear ramps
 * Hardware: PIC18F252, L6219
 *
 * Compile with on Microchip XC8 compiler with the command line:
 * XC8 --chip=18F252 motor_test3.c
 *
 * Copyright (c) 2015 Robert Ramey
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <xc.h>

// 8 MHz internal clock
#define _XTAL_FREQ 8000000

#include <stdint.h>
#include <stdbool.h>        /* For true/false definition */

// ***************************
// nullify macro for literals
#define literal(n) n

// ***************************
// map strong types to appropriate underlying types.
typedef uint16_t position_t;
typedef int16_t step_t;
typedef uint32_t ccpr_t;
typedef int32_t c_t;
typedef uint16_t phase_t;
typedef uint8_t phase_ix_t;
typedef int8_t direction_t;

// 1st step=10ms; max speed=300rpm (based on 1MHz timer, 1.8deg steps)
#define C_MIN  literal((1000 << 8))
#define C0    literal((10000 << 8))

#include "motor3.c"

void main() {
    initialize();
    // move motor to position 200
    motor_run(200);
    while(busy()) __delay_ms(100);
    // move motor to position 1000
    motor_run(1000);
    while(busy()) __delay_ms(100);
    // move back to position 0
    motor_run(200);
    while(busy()) __delay_ms(100);
} // main()
