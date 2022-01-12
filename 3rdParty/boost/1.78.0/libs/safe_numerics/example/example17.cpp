//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/safe_numerics/safe_integer.hpp>
#include <cstdint> // uint8_t
using namespace boost::safe_numerics;

uint8_t f(uint8_t i){
    return i;
}

using safe_t = safe<long, native, loose_trap_policy>;

int main(){
    const long x = 97;
    f(x);   // OK - implicit conversion to int can never fail
    const safe_t y = 97;
    f(y);   // could overflow so trap at compile time
    return 0;
}
