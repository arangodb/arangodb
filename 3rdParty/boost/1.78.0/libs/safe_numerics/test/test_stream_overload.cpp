//  Copyright (c) 2021 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test stream overload of >> and << for some non-numeric type

// this is a compile only test - but since many build systems
#include <boost/safe_numerics/safe_integer.hpp>

struct X {};

using I = boost::safe_numerics::safe<int64_t>;

void operator>>(X, I){};
void operator<<(X, I){};


void f(X x, I i) {
    x << i;
    x >> i;
}

