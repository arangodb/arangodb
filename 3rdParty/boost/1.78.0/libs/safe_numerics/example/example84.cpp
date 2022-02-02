//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <stdexcept>
#include <iostream>
#include <sstream>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_range.hpp>
#include <boost/safe_numerics/native.hpp>
#include <boost/safe_numerics/exception.hpp>

#include "safe_format.hpp" // prints out range and value of any type

using namespace boost::safe_numerics;

using safe_t = safe_signed_range<
    -24,
    82,
    native,
    loose_exception_policy
>;

// define variables used for input
using input_safe_t = safe_signed_range<
    -24,
    82,
    native, // we don't need automatic in this case
    loose_exception_policy // assignment of out of range value should throw
>;

// function arguments can never be outside of limits
auto f(const safe_t & x, const safe_t & y){
    auto z = x + y;  // we know that this cannot fail
    std::cout << "z = " << safe_format(z) << std::endl;
    std::cout << "(x + y) = " << safe_format(x + y) << std::endl;
    std::cout << "(x - y) = " << safe_format(x - y) << std::endl;
    return z;
}

bool test(const char * test_string){
    std::istringstream sin(test_string);
    input_safe_t x, y;
    try{
        std::cout << "type in values in format x y:" << test_string << std::endl;
        sin >> x >> y; // read varibles, maybe throw exception
    }
    catch(const std::exception & e){
        // none of the above should trap. Mark failure if they do
        std::cout << e.what() << '\n' << "input failure" << std::endl;
        return false;
    }
    std::cout << "x" << safe_format(x) << std::endl;
    std::cout << "y" << safe_format(y) << std::endl;
    std::cout << safe_format(f(x, y)) << std::endl;
    std::cout << "input success" << std::endl;
    return true;
}

int main(){
    std::cout << "example 84:\n";
    bool result =
        !  test("123 125")
        && test("0 0")
        && test("-24 82")
    ;
    std::cout << (result ? "Success!" : "Failure") << std::endl;
    return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
