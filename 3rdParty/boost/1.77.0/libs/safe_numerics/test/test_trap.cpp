//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// testing trap

// this is a compile only test - but since many build systems
// can't handle a compile-only test - make sure it passes trivially.

#include <boost/safe_numerics/exception_policies.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

using namespace boost::safe_numerics;
template <typename T> // T is char, int, etc data type
using safe_t = safe<
    T,
    native,
    loose_trap_policy // use for compiling and running tests
>;

template<typename T, typename U>
void test(){
    safe_t<T> t;
    safe_t<U> u;
    t + u;
    t - u;
    t * u;
    t / u;  // could fail regardless of data type
    t % u;  // could fail regardless of data type
    t << u;
    t >> u;
    t | u;
    t & u;
    t ^ u;
}
int main(int, char *[]){
    test<std::int8_t, std::int8_t>();   // should compile
    test<std::int16_t, std::int16_t>(); // should compile
    test<std::int32_t, std::int32_t>(); // should fail to compile
    test<std::int64_t, std::int64_t>(); // should fail to compile
    return 0;
}
