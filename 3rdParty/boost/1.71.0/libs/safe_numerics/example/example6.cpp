#include <stdexcept>
#include <sstream>
#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    // problem: checking of externally produced value can be overlooked
    std::cout << "example 6: ";
    std::cout << "checking of externally produced value can be overlooked" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;

    std::istringstream is("12317289372189 1231287389217389217893");

    try{
        int x, y;
        is >> x >> y; // get integer values from the user
        std::cout << x << ' ' << y << std::endl;
        std::cout << "error NOT detected!" << std::endl;
    }
    catch(const std::exception &){
        std::cout << "error detected!" << std::endl;
    }

    // solution: assign externally retrieved values to safe equivalents
    std::cout << "Using safe numerics" << std::endl;
    {
        using namespace boost::safe_numerics;
        safe<int> x, y;
        is.seekg(0);
        try{
            is >> x >> y; // get integer values from the user
            std::cout << x << ' ' << y << std::endl;
            std::cout << "error NOT detected!" << std::endl;
        }
        catch(const std::exception & e){
            std::cout << "error detected:"  << e.what() << std::endl;
        }
    }
    return 0;
}
