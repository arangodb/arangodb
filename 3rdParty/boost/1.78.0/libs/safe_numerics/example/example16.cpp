//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/safe_numerics/safe_integer.hpp>
using namespace boost::safe_numerics;

int f(int i){
    return i;
}

using safe_t = safe<long>;

int main(){
    const long x = 97;
    f(x);   // OK - implicit conversion to int
    const safe_t y = 97;
    f(y);   // Also OK - checked implicit conversion to int
    return 0;
}