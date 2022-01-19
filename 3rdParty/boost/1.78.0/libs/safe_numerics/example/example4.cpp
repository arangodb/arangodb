//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

int main(){
    std::cout << "example 4: ";
    std::cout << "implicit conversions change data values" << std::endl;
    std::cout << "Not using safe numerics" << std::endl;
    
    // problem: implicit conversions change data values
    try{
        signed int   a{-1};
        unsigned int b{1};
        std::cout << "a is " << a << " b is " << b << '\n';
        if(a < b){
            std::cout << "a is less than b\n";
        }
        else{
            std::cout << "b is less than a\n";
        }
        std::cout << "error NOT detected!" << std::endl;
    }
    catch(const std::exception &){
        // never arrive here - just produce the wrong answer!
        std::cout << "error detected!" << std::endl;
        return 1;
    }

    // solution: replace int with safe<int> and unsigned int with safe<unsigned int>
    std::cout << "Using safe numerics" << std::endl;
    try{
        using namespace boost::safe_numerics;
        safe<signed int>   a{-1};
        safe<unsigned int> b{1};
        std::cout << "a is " << a << " b is " << b << '\n';
        if(a < b){
            std::cout << "a is less than b\n";
        }
        else{
            std::cout << "b is less than a\n";
        }
        std::cout << "error NOT detected!" << std::endl;
        return 1;
    }
    catch(const std::exception & e){
        // never arrive here - just produce the correct answer!
        std::cout << e.what() << std::endl;
        std::cout << "error detected!" << std::endl;
    }
    return 0;
}
