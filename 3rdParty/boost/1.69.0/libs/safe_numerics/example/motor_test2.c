/*
 * david austin
 * http://www.embedded.com/design/mcus-processors-and-socs/4006438/Generate-stepper-motor-speed-profiles-in-real-time
 * DECEMBER 30, 2004
 *
 * Demo program for stepper motor control with linear ramps
 * Hardware: PIC18F252, L6219
 *
 * Compile with on Microchip XC8 compiler with the command line:
 * XC8 --chip=18F252 motor_test2.c
 *
 * Copyright (c) 2015 Robert Ramey
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>        /* For true/false definition */

// *************************** 
// alias integer types standard C integer types
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

// 1st step=50ms; max speed=120rpm (based on 1MHz timer, 1.8deg steps)
#define C0    (50000*8l)
#define C_MIN  (2500*8)

#include "motor2.c"

void main() {
    initialize();
     while (1) { // repeat 5 revs forward & back
        motor_run(1000);
        while (run_flg);
        motor_run(0);
        while (run_flg);
        motor_run(50000);
        while (run_flg);
    }
} // main()
