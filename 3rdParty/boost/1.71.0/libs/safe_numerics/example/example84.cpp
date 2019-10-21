#include <stdexcept>
#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_range.hpp>
#include <boost/safe_numerics/automatic.hpp>
#include <boost/safe_numerics/exception.hpp>

#include "safe_format.hpp" // prints out range and value of any type

using namespace boost::safe_numerics;

using safe_t = safe_signed_range<
    -24,
    82,
    automatic,
    loose_trap_policy
>;

// define variables used for input
using input_safe_t = safe_signed_range<
    -24,
    82,
    automatic, // we don't need automatic in this case
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

int main(int argc, const char * argv[]){
    std::cout << "example 84:\n";
    input_safe_t x, y;
    try{
        std::cout << "type in values in format x y:" << std::flush;
        std::cin >> x >> y; // read varibles, maybe throw exception
    }
    catch(const std::exception & e){
        // none of the above should trap. Mark failure if they do
        std::cout << e.what() << std::endl;
        return 1;
    }
    std::cout << "x" << safe_format(x) << std::endl;
    std::cout << "y" << safe_format(y) << std::endl;
    std::cout << safe_format(f(x, y)) << std::endl;
    return 0;
}
