//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// testing floating point

// this is a compile only test - but since many build systems
// can't handle a compile-only test - make sure it passes trivially.
#include <cassert>
#include <boost/safe_numerics/safe_integer.hpp>

template<typename T, typename U>
void test(){
    T t;
    U u;
    float x = t;
    t = x;
    t + u;
    t - u;
    t * u;
    t / u;
/**/
    // the operators below are restricted to integral types
}
int main(){
    using namespace boost::safe_numerics;
    /*
    test<safe<std::int8_t>, float>();
    test<safe<std::int16_t>,float>();
    test<safe<std::int32_t>, float>();
    test<safe<std::int64_t>, float>();
    */
    return 0;
}
