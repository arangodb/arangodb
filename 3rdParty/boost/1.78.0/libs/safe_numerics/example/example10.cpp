//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <cstdint>

#include <boost/safe_numerics/safe_integer.hpp>

using namespace std;
using namespace boost::safe_numerics;

void f(const unsigned int & x, const int8_t & y){
    cout << x * y << endl;
}
void safe_f(
    const safe<unsigned int> & x,
    const safe<int8_t> & y
){
    cout << x * y << endl;
}

int main(){
    cout << "example 4: ";
    cout << "mixing types produces surprising results" << endl;
    try {
        std::cout << "Not using safe numerics" << std::endl;
        // problem: mixing types produces surprising results.
        f(100, 100);  // works as expected
        f(100, -100); // wrong result - unnoticed
        cout << "error NOT detected!" << endl;;
    }
    catch(const std::exception & e){
        // never arrive here
        cout << "error detected:" << e.what() << endl;;
    }
    try {
        // solution: use safe types
        std::cout << "Using safe numerics" << std::endl;
        safe_f(100, 100);  // works as expected
        safe_f(100, -100); // throw error
        cout << "error NOT detected!" << endl;;
    }
    catch(const std::exception & e){
        cout << "error detected:" << e.what() << endl;;
    }
    return 0;
}

