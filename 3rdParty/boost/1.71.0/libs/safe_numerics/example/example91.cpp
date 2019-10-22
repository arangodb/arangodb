//////////////////////////////////////////////////////////////////
// example91.cpp
//
// Copyright (c) 2015 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <limits>

#include <boost/safe_numerics/cpp.hpp>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics//safe_integer_range.hpp>

// use same type promotion as used by the pic compiler
// see the following comment in motor.c
// Types: int8,int16,int32=8,16,32bit integers

using pic16_promotion = boost::safe_numerics::cpp<
    8,  // char
    8,  // short
    8,  // int
    16, // long
    32  // long long
>;

// define safe types used desktop version of the program.  In conjunction
// with the promotion policy above, this will permit us to guarantee that
// the resulting program will be free of arithmetic errors introduced by
// C expression syntax and type promotion with no runtime penalty
template <typename T> // T is char, int, etc data type
using safe_t = boost::safe_numerics::safe<
    T,
    pic16_promotion,
    boost::safe_numerics::default_exception_policy // use for compiling and running tests
>;
using safe_bool_t = boost::safe_numerics::safe_unsigned_range<
    0,
    1,
    pic16_promotion,
    boost::safe_numerics::default_exception_policy // use for compiling and running tests
>;

#define DESKTOP
#include "motor1.c"

#include <chrono>
#include <thread>

void sleep(int16){
    std::this_thread::sleep_for(std::chrono::microseconds(ccpr));
}

int main(){
    std::cout << "start test\n";
    try{
        initialize();
        motor_run(100);
        do{
            isr_motor_step();
        }while (run_flg);

        // move motor to position 1000
        motor_run(1000);
        do{
            sleep(ccpr);
            isr_motor_step();
        }while (run_flg);

        // move back to position 0
        motor_run(0);
        do{
            sleep(ccpr);
            isr_motor_step();
        }while (run_flg);
    }
    catch(std::exception & e){
        std::cout << e.what() << '\n';
        // we expect to trap an exception
        return 0;
    }
    catch(...){
        std::cout << "test interrupted\n";
        return 1;
    }
    std::cout << "end test\n";
    return 1;
} 
