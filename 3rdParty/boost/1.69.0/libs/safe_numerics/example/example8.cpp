#include <cassert>
#include <exception>
#include <iostream>
#include <cstdint>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    std::cout << "example 8:";
    std::cout << "undetected erroneous expression evaluation" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;
    try{
        unsigned int x = 127;
        unsigned int y = 2;
        unsigned int z;
        // this produces an invalid result !
        z = y - x;
        std::cout << "error NOT detected!" << std::endl;
        std::cout << z << " != " << y << " - " << x << std::endl;
    }
    catch(std::exception){
        std::cout << "error detected!" << std::endl;
    }
    // solution: replace int with safe<int>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<unsigned int> x = 127;
        safe<unsigned int> y = 2;
        safe<unsigned int> z;
        // rather than producing an invalid result an exception is thrown
        z = y - x;
        std::cout << "error NOT detected!" << std::endl;
        std::cout << z << " != " << y << " - " << x << std::endl;
    }
    catch(std::exception & e){
        // which we can catch here
        std::cout << "error detected:" << e.what() << std::endl;
    }
    return 0;
}
