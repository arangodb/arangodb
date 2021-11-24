// Copyright 2016-2021 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

//[pfr_example_get
/*`
    The following example shows how to access structure fields by index using [funcref boost::pfr::get].

    Let's define some structure:
*/
#include <boost/pfr/core.hpp>

struct foo {            // defining structure
    int some_integer;
    char c;
};

/*`
    We can access fields of that structure by index:
*/
foo f {777, '!'};
auto& r1 = boost::pfr::get<0>(f); // accessing field with index 0, returns reference to `foo::some_integer`
auto& r2 = boost::pfr::get<1>(f); // accessing field with index 1, returns reference to `foo::c`
//] [/pfr_example_get]


int main() {
    if (r1 != 777) return 1;
    if (r2 != '!') return 2;
    
    r1 = 42;
    r2 = 'A';

    if (r1 != 42) return 3;
    if (r2 != 'A') return 4;
    if (f.some_integer != 42) return 5;
    if (f.c != 'A') return 6;

    return 0;
}
