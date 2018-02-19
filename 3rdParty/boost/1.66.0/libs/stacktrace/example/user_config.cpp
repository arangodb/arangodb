// Copyright Antony Polukhin, 2016-2017.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_USER_CONFIG <libs/stacktrace/example/user_config.hpp>

#include <boost/array.hpp>
#include <exception>    // std::set_terminate, std::abort
#include <boost/stacktrace.hpp>
#include <iostream>     // std::cerr
BOOST_NOINLINE void foo(int i);
BOOST_NOINLINE void bar(int i);
 
BOOST_NOINLINE void bar(int i) {
    boost::array<int, 5> a = {{-1, -231, -123, -23, -32}};
    if (i >= 0) {
        foo(a[i]);
    } else {
        std::cerr << "Terminate called:\n" << boost::stacktrace::stacktrace() << '\n';
        std::exit(0);
    }
}

BOOST_NOINLINE void foo(int i) {
    bar(--i);
}

int main() {
    foo(5);
    
    return 2;
}




