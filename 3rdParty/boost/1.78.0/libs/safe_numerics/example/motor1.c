/*
 * david austin
 * http://www.embedded.com/design/mcus-processors-and-socs/4006438/Generate-stepper-motor-speed-profiles-in-real-time
 * DECEMBER 30, 2004
 *
 * Demo program for stepper motor control with linear ramps
 * Hardware: PIC18F252, L6219
 *
 * Copyright (c) 2015 Robert Ramey
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

// ramp state-machine states
enum ramp_state {
    ramp_idle = 0,
    ramp_up = 1,
    ramp_max = 2,
    ramp_down = 3,
    ramp_last = 4,
};

enum ramp_state ramp_sts=ramp_idle;
int16 motor_pos = 0;    // absolute step number
int16 pos_inc=0;        // motor_pos increment
uint16 phase=0;         // ccpPhase[phase_ix]
uint8  phase_ix=0;      // index to ccpPhase[]
uint8  phase_inc;       // phase_ix increment
uint8  run_flg;         // true while motor is running
uint16 ccpr;            // copy of CCPR1&2
uint16 c;               // integer delay count
uint16 step_no;          // progress of move
uint16 step_down;       // start of down-ramp
uint16 move;             // total steps to move
uint16 midpt;           // midpoint of move
uint32 c32;              // 24.8 fixed point delay count
int16 denom;            // 4.n+1 in ramp algo

// Config data to make CCP1&2 generate quadrature sequence on PHASE pins
// Action on CCP match: 8=set+irq; 9=clear+irq
uint16 const ccpPhase[] = {0x909, 0x908, 0x808, 0x809}; // 00,01,11,10

void current_on(){/* code as needed */}  // motor drive current
void current_off(){/* code as needed */} // reduce to holding value

uint16 make16(uint8 l, uint8 r) {
    return (uint16) (l << 8) + r;
}

// compiler-specific ISR declaration

void __interrupt isr_motor_step(void) { // CCP1 match -> step pulse + IRQ
    ccpr += c; // next comparator value
    switch (ramp_sts) {
        case ramp_up: // accel
            if (step_no == midpt) { // midpoint: decel
                ramp_sts = ramp_down;
                denom = ((step_no - move) << 2) + 1;
                if (!(move & 1)) { // even move: repeat last delay before decel
                    denom += 4;
                    break;
                }
            }
            // no break: share code for ramp algo
        case ramp_down: // decel
            if (step_no == move - 1) { // next irq is cleanup (no step)
                ramp_sts = ramp_last;
                break;
            }
            denom += 4;
            c32 -= (c32 << 1) / denom; // ramp algorithm
            // beware confict with foreground code if long div not reentrant
            c = (c32 + 128) >> 8; // round 24.8format->int16
            if (c <= C_MIN) { // go to constant speed
                ramp_sts = ramp_max;
                step_down = move - step_no;
                c = C_MIN;
                break;
            }
            break;
        case ramp_max: // constant speed
            if (step_no == step_down) { // start decel
                ramp_sts = ramp_down;
                denom = ((step_no - move) << 2) + 5;
            }
            break;
        default: // last step: cleanup
            ramp_sts = ramp_idle;
            current_off(); // reduce motor current to holding value
            CCP1IE = 0; // disable_interrupts(INT_CCP1);
            run_flg = false; // move complete
            break;
    } // switch (ramp_sts)
    if (ramp_sts != ramp_idle) {
        motor_pos += pos_inc;
        ++step_no;
        CCPR2H = CCPR1H = (ccpr >> 8); // timer value at next CCP match
        CCPR2L = CCPR1L = (ccpr & 0xff);
        if (ramp_sts != ramp_last) // else repeat last action: no step
            phase_ix = (phase_ix + phase_inc) & 3;
        phase = ccpPhase[phase_ix];
        CCP1CON = phase & 0xff; // set CCP action on next match
        CCP2CON = phase >> 8;
    } // if (ramp_sts != ramp_idle)
} // isr_motor_step()

void motor_run(int16 pos_new) { // set up to drive motor to pos_new (absolute step#)
    if (pos_new < motor_pos) { // get direction & #steps
        move = motor_pos - pos_new;
        pos_inc = -1;
        phase_inc = 0xff;
    } 
    else if (pos_new != motor_pos) {
        move = pos_new - motor_pos;
        pos_inc = 1;
        phase_inc = 1;
    } else return; // already there
    midpt = (move - 1) >> 1;
    c = C0;
    c32 = c << 8; // keep c in 24.8 fixed-point format for ramp calcs
    step_no = 0; // step counter
    denom = 1; // 4.n+1, n=0
    ramp_sts = ramp_up; // start ramp state-machine
    run_flg = true;
    T1CONbits.TMR1ON = 0; // stop timer1;
    ccpr = make16(TMR1H, TMR1L); // 16bit value of Timer1
    ccpr += 1000; // 1st step + irq 1ms after timer1 restart
    CCPR2H = CCPR1H = (ccpr >> 8);
    CCPR2L = CCPR1L = (ccpr & 0xff);
    phase_ix = (phase_ix + phase_inc) & 3;
    phase = ccpPhase[phase_ix];
    CCP1CON = phase & 0xff; // sets action on match
    CCP2CON = phase >> 8;
    current_on(); // current in motor windings
    CCP1IE = 1; // enable_interrupts(INT_CCP1);
    T1CONbits.TMR1ON = 1; // restart timer1;
} // motor_run()

void initialize() {
    di();         // disable_interrupts(GLOBAL);
    CCP1IE = 0;   // disable_interrupts(INT_CCP1);
    CCP2IE = 0;   // disable_interrupts(INT_CCP2);
    PORTC = 0;    // output_c(0);
    TRISC = 0;    // set_tris_c(0);
    T3CON = 0;
    T1CON = 0x35;
    INTCONbits.PEIE = 1;
    INTCONbits.RBIF = 0;
    ei();         // enable_interrupts(GLOBAL);
} // initialize()
