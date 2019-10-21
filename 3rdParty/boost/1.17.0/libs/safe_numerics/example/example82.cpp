#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/exception_policies.hpp>
#include <boost/safe_numerics/automatic.hpp>
#include "safe_format.hpp" // prints out range and value of any type

using safe_t = boost::safe_numerics::safe<
    int,
    boost::safe_numerics::automatic, // note use of "automatic" policy!!!
    boost::safe_numerics::loose_trap_policy
>;

int main(int, const char *[]){
    std::cout << "example 82:\n";
    safe_t x(INT_MAX);
    safe_t y = 2;
    std::cout << "x = " << safe_format(x) << std::endl;
    std::cout << "y = " << safe_format(y) << std::endl;
    std::cout << "x + y = " << safe_format(x + y) << std::endl;
    return 0;
}

