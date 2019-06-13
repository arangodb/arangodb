#include <stdexcept>
#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    // problem: cannot recover from arithmetic errors
    std::cout << "example 7: ";
    std::cout << "cannot recover from arithmetic errors" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;

    try{
        int x = 1;
        int y = 0;
        // can't do this as it will crash the program with no
        // opportunity for recovery - comment out for example
        //std::cout << x / y;
        std::cout << "error NOT detectable!" << std::endl;
    }
    catch(std::exception){
        std::cout << "error detected!" << std::endl;
    }

    // solution: replace int with safe<int>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<int> x = 1;
        safe<int> y = 0;
        std::cout << x / y;
        std::cout << " error NOT detected!" << std::endl;
    }
    catch(std::exception & e){
        std::cout << "error detected:" << e.what() << std::endl;
    }
    return 0;
}
