//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    std::cout << "example 3:";
    std::cout << "undetected underflow in data type" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;
    // problem: decrement can yield incorrect result
    try{
        unsigned int x = 0;
        // the following silently produces an incorrect result
        --x;
        std::cout << x << " != " << -1 << std::endl;

        // when comparing int and unsigned int, C++ converts
        // the int to unsigned int so the following assertion
        // fails to detect the above error!
        assert(x == -1);

        std::cout << "error NOT detected!" << std::endl;
    }
    catch(const std::exception &){
        // never arrive here
        std::cout << "error detected!" << std::endl;
    }
    // solution: replace unsigned int with safe<unsigned int>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<unsigned int> x = 0;
        // decrement unsigned to less than zero throws exception
        --x;
        assert(false); // never arrive here
    }
    catch(const std::exception & e){
        std::cout << e.what() << std::endl;
        std::cout << "error detected!" << std::endl;
    }
    return 0;
}
