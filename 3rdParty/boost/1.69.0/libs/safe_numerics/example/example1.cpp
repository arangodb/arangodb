#include <cassert>
#include <exception>
#include <iostream>
#include <cstdint>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    std::cout << "example 1:";
    std::cout << "undetected erroneous expression evaluation" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;
    try{
        std::int8_t x = 127;
        std::int8_t y = 2;
        std::int8_t z;
        // this produces an invalid result !
        z = x + y;
        std::cout << "error NOT detected!" << std::endl;
        std::cout << (int)z << " != " << (int)x << " + " << (int)y << std::endl;
    }
    catch(std::exception){
        std::cout << "error detected!" << std::endl;
    }
    // solution: replace int with safe<int>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<std::int8_t> x = INT_MAX;
        safe<std::int8_t> y = 2;
        safe<std::int8_t> z;
        // rather than producing an invalid result an exception is thrown
        z = x + y;
    }
    catch(std::exception & e){
        // which we can catch here
        std::cout << "error detected:" << e.what() << std::endl;
    }
    return 0;
}
