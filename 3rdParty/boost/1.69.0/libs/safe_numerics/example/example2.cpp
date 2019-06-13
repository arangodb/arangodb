#include <cassert>
#include <exception>
#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    std::cout << "example 2:";
    std::cout << "undetected overflow in data type" << std::endl;
    // problem: undetected overflow
    std::cout << "Not using safe numerics" << std::endl;
    try{
        int x = INT_MAX;
        // the following silently produces an incorrect result
        ++x;
        std::cout << x << " != " << INT_MAX << " + 1" << std::endl;
        std::cout << "error NOT detected!" << std::endl;
    }
    catch(std::exception){
        std::cout << "error detected!" << std::endl;
    }
    // solution: replace int with safe<int>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<int> x = INT_MAX;
        // throws exception when result is past maximum possible 
        ++x;
        assert(false); // never arrive here
    }
    catch(std::exception & e){
        std::cout << e.what() << std::endl;
        std::cout << "error detected!" << std::endl;
    }
    return 0;
}
