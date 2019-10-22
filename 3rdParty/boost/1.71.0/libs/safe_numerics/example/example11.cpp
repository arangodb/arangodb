//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char * []){
    std::cout << "example 1:";
    std::cout << "undetected erroneous expression evaluation" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;
    // problem: arithmetic operations can yield incorrect results.
    try{
        std::int8_t x = 127;
        std::int8_t y = 2;
        std::int8_t z;
        // this produces an invalid result !
        z = x + y;
        std::cout << z << " != " << x + y << std::endl;
        std::cout << "error NOT detected!" << std::endl;
    }
    catch(const std::exception &){
        std::cout << "error detected!" << std::endl;
    }
    // solution: replace std::int8_t with safe<std::int8_t>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<std::int8_t> x = 127;
        safe<std::int8_t> y = 2;
        // rather than producing and invalid result an exception is thrown
        safe<std::int8_t> z = x + y;
    }
    catch(const std::exception & e){
        // which can catch here
        std::cout << e.what() << std::endl;
    }
    return 0;
}
