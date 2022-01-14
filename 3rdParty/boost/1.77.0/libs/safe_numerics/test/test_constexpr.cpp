//  Copyright (c) 2014 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// Test constexpr operations on literals

#include <iostream>

#include <boost/safe_numerics/safe_integer_literal.hpp>
#include <boost/safe_numerics/native.hpp>
#include <boost/safe_numerics/exception.hpp>

using namespace boost::safe_numerics;

template<std::uintmax_t N>
using compile_time_value = safe_unsigned_literal<N, native, loose_trap_policy>;

int main(){
    constexpr const compile_time_value<1000> x;
    constexpr const compile_time_value<1> y;

    // should compile and execute without problem
    std::cout << x << '\n';

    // all the following statements should compile
    constexpr auto x_plus_y = x + y;
    static_assert(1001 == x_plus_y, "1001 == x + y");

    constexpr auto x_minus_y = x - y;
    static_assert(999 == x_minus_y, "999 == x - y");

    constexpr auto x_times_y = x * y;
    static_assert(1000 == x_times_y, "1000 == x * y");

    constexpr auto x_and_y = x & y;
    static_assert(0 == x_and_y, "0 == x & y");

    constexpr auto x_or_y = x | y;
    static_assert(1001 == x_or_y, "1001 == x | y");

    constexpr auto x_xor_y = x ^ y;
    static_assert(1001 == x_xor_y, "1001 == x ^ y");

    constexpr auto x_divided_by_y = x / y;
    static_assert(1000 == x_divided_by_y, "1000 == x / y");

    constexpr auto x_mod_y = x % y;
    static_assert(0 == x_mod_y, "0 == x % y");

    // this should fail compilation since a positive unsigned number
    // can't be converted to a negative value of the same type
    constexpr auto minus_x = -x;

    // should compile OK since the negative inverse of a zero is still zero
    constexpr const compile_time_value<0> x0;
    constexpr auto minus_x0 = -x0; // should compile OK
    static_assert(0 == minus_x0, "0 == -x where x == 0");

    constexpr auto not_x = ~x;

    return 0;
}
