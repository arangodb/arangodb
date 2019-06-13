#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/exception_policies.hpp> // include exception policies

using safe_t = boost::safe_numerics::safe<
    int,
    boost::safe_numerics::native,
    boost::safe_numerics::loose_trap_policy  // note use of "loose_trap_exception" policy!
>;

int main(int argc, const char * argv[]){
    std::cout << "example 81:\n";
    safe_t x(INT_MAX);
    safe_t y(2);
    safe_t z = x + y; // will fail to compile !
    return 0;
}
