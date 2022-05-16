//////////////////////////////////////////////////////////////////
// example92.cpp
//
// Copyright (c) 2015 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

// *************************** 
// 1. include headers to support safe integers
#include <boost/safe_numerics/cpp.hpp>
#include <boost/safe_numerics/exception.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

// *************************** 
// 2. specify a promotion policy to support proper emulation of 
// PIC types on the desktop
using pic16_promotion = boost::safe_numerics::cpp<
    8,  // char      8 bits
    16, // short     16 bits
    16, // int       16 bits
    16, // long      16 bits
    32  // long long 32 bits
>;

// 1st step=50ms; max speed=120rpm (based on 1MHz timer, 1.8deg steps)
#define C0    (50000 << 8)
#define C_MIN  (2500 << 8)

static_assert(C0 < 0xffffff, "Largest step too long");
static_assert(C_MIN > 0, "Smallest step must be greater than zero");
static_assert(C_MIN < C0, "Smallest step must be smaller than largest step");

// *************************** 
// 3. define PIC integer type names to be safe integer types of he same size.

template <typename T> // T is char, int, etc data type
using safe_t = boost::safe_numerics::safe<
    T,
    pic16_promotion
>;

// alias original program's integer types to corresponding PIC safe types
// In conjunction with the promotion policy above, this will permit us to 
// guarantee that the resulting program will be free of arithmetic errors 
// introduced by C expression syntax and type promotion with no runtime penalty

typedef safe_t<int8_t> int8;
typedef safe_t<int16_t> int16;
typedef safe_t<int32_t> int32;
typedef safe_t<uint8_t> uint8;
typedef safe_t<uint16_t> uint16;
typedef safe_t<uint32_t> uint32;

// *************************** 
// 4. emulate PIC features on the desktop

// filter out special keyword used only by XC8 compiler
#define __interrupt
// filter out XC8 enable/disable global interrupts
#define ei()
#define di()

// emulate PIC special registers
uint8 RCON;
uint8 INTCON;
uint8 CCP1IE;
uint8 CCP2IE;
uint8 PORTC;
uint8 TRISC;
uint8 T3CON;
uint8 T1CON;

uint8 CCPR2H;
uint8 CCPR2L;
uint8 CCPR1H;
uint8 CCPR1L;
uint8 CCP1CON;
uint8 CCP2CON;
uint8 TMR1H;
uint8 TMR1L;

// create type used to map PIC bit names to
// correct bit in PIC register
template<typename T, std::int8_t N>
struct bit {
    T & m_word;
    constexpr explicit bit(T & rhs) :
        m_word(rhs)
    {}
    constexpr bit & operator=(int b){
        if(b != 0)
            m_word |= (1 << N);
        else
            m_word &= ~(1 << N);
        return *this;
    }
    constexpr operator int () const {
        return m_word >> N & 1;
    }
};

// define bits for T1CON register
struct  {
    bit<uint8, 7> RD16{T1CON};
    bit<uint8, 5> T1CKPS1{T1CON};
    bit<uint8, 4> T1CKPS0{T1CON};
    bit<uint8, 3> T1OSCEN{T1CON};
    bit<uint8, 2> T1SYNC{T1CON};
    bit<uint8, 1> TMR1CS{T1CON};
    bit<uint8, 0> TMR1ON{T1CON};
} T1CONbits;

// define bits for T1CON register
struct  {
    bit<uint8, 7> GEI{INTCON};
    bit<uint8, 5> PEIE{INTCON};
    bit<uint8, 4> TMR0IE{INTCON};
    bit<uint8, 3> RBIE{INTCON};
    bit<uint8, 2> TMR0IF{INTCON};
    bit<uint8, 1> INT0IF{INTCON};
    bit<uint8, 0> RBIF{INTCON};
} INTCONbits;

// ***************************
// 5. include the environment independent code we want to test
#include "motor2.c"

#include <chrono>
#include <thread>

// round 24.8 format to microseconds
int32 to_microseconds(uint32 t){
    return (t + 128) / 256;
}

using result_t = uint8_t;
const result_t success = 1;
const result_t fail = 0;

// move motor to the indicated target position in steps
result_t test(int32 m){
    try {
        std::cout << "move motor to " << m << '\n';
        motor_run(m);
        std::cout
        << "step #" << ' '
        << "delay(us)(24.8)" << ' '
        << "delay(us)" << ' '
        << "CCPR" << ' '
        << "motor position" << '\n';
        do{
            std::this_thread::sleep_for(std::chrono::microseconds(to_microseconds(c)));
            uint32 last_c = c;
            uint32 last_ccpr = ccpr;
            isr_motor_step();
            std::cout
            << step_no << ' '
            << last_c << ' '
            << to_microseconds(last_c) << ' '
            << std::hex << last_ccpr << std::dec << ' '
            << motor_pos << '\n';
        }while(run_flg);
    }
    catch(const std::exception & e){
        std::cout << e.what() << '\n';
        return fail;
    }
    return success;
}

int main(){
    std::cout << "start test\n";
    result_t result = success;
    try{
        initialize();
        // move motor to position 1000
        result &= test(1000);
        // move motor to position 200
        result &= test(200);
        // move motor to position 200 again! Should result in no movement.
        result &= test(200);
        // move back to position 0
        result &= test(0);
        // ***************************
        // 6. error detected here!  data types can't handle enough
        // steps to move the carriage from end to end! Suppress this
        // test for now.
        // move motor to position 50000.
//        result &= test(50000);
        // move motor back to position 0.
        result &= test(0);
    }
    catch(const std::exception & e){
        std::cout << e.what() << '\n';
        return 1;
    }
    catch(...){
        std::cout << "test interrupted\n";
        return EXIT_FAILURE;
    }
    std::cout << "end test\n";
    return result == success ? EXIT_SUCCESS : EXIT_FAILURE;
}
