#include <stdexcept>
#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    // problem: cannot recover from arithmetic errors
    std::cout << "example 8: ";
    std::cout << "cannot detect compile time arithmetic errors" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;

    try{
        const int x = 1;
        const int y = 0;
        // will emit warning at compile time
        // will leave an invalid result at runtime.
        std::cout << x / y; // will display "0"!
        std::cout << "error NOT detected!" << std::endl;
    }
    catch(const std::exception &){
        std::cout << "error detected!" << std::endl;
    }
    // solution: replace int with safe<int>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        const safe<int> x = 1;
        const safe<int> y = 0;
        // constexpr const safe<int> z = x / y; // note constexpr here!
        std::cout << x / y; // error would be detected at runtime
        std::cout << " error NOT detected!" << std::endl;
    }
    catch(const std::exception & e){
        std::cout << "error detected:" << e.what() << std::endl;
    }
    return 0;
}
