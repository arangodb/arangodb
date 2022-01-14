// Copyright Antony Polukhin, 2016-2021.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_ENABLE_ASSERT_HANDLER

#include <cstdlib> // std::exit
#include <boost/array.hpp>
BOOST_NOINLINE void foo(int i);
BOOST_NOINLINE void bar(int i);
 
BOOST_NOINLINE void bar(int i) {
    boost::array<int, 5> a = {{101, 100, 123, 23, 32}};
    if (i >= 0) {
        foo(a[i]);
    } else {
        std::exit(1);
    }
}

BOOST_NOINLINE void foo(int i) {
    bar(--i);
}

//[getting_started_assert_handlers

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER is defined for the whole project
#include <stdexcept>    // std::logic_error
#include <iostream>     // std::cerr
#include <boost/stacktrace.hpp>

namespace boost {
    inline void assertion_failed_msg(char const* expr, char const* msg, char const* function, char const* /*file*/, long /*line*/) {
        std::cerr << "Expression '" << expr << "' is false in function '" << function << "': " << (msg ? msg : "<...>") << ".\n"
            << "Backtrace:\n" << boost::stacktrace::stacktrace() << '\n';
        /*<-*/ std::exit(0); /*->*/
        /*=std::abort();*/
    }

    inline void assertion_failed(char const* expr, char const* function, char const* file, long line) {
        ::boost::assertion_failed_msg(expr, 0 /*nullptr*/, function, file, line);
    }
} // namespace boost
//]


int main() {
    foo(5);
    
    return 2;
}


