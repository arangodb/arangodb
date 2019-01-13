#include <cassert>
#include <exception>
#include <iostream>
#include <cstdint>

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
    catch(std::exception){
        std::cout << "error detected!" << std::endl;
    }
    // solution: replace std::int8_t with safe<std::int8_t>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<std::int8_t> x = 127;
        safe<std::int8_t> y = 2;
        safe<std::int8_t> z;
        // rather than producing and invalid result an exception is thrown
        z = x + y;
    }
    catch(std::exception & e){
        // which can catch here
        std::cout << e.what() << std::endl;
    }
    return 0;
}
