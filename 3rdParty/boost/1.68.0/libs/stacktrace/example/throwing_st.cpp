// Copyright Antony Polukhin, 2016-2018.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//[getting_started_class_traced
#include <boost/stacktrace.hpp>
#include <boost/exception/all.hpp> 

typedef boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace> traced;
//]

//[getting_started_class_with_trace
template <class E>
void throw_with_trace(const E& e) {
    throw boost::enable_error_info(e)
        << traced(boost::stacktrace::stacktrace());
} 
//]

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_NOINLINE void oops(int i);
BOOST_NOINLINE void foo(int i);
BOOST_NOINLINE void bar(int i);

#include <stdexcept>
BOOST_NOINLINE void oops(int i) {
    //[getting_started_throwing_with_trace
    if (i >= 4)
        throw_with_trace(std::out_of_range("'i' must be less than 4 in oops()"));
    if (i <= 0)
        throw_with_trace(std::logic_error("'i' must not be greater than zero in oops()"));
    //]
    foo(i);
    std::exit(1);
}

#include <boost/array.hpp>
BOOST_NOINLINE void bar(int i) {
    boost::array<int, 5> a = {{0, 0, 0, 0, 0}};
    if (i < 5) {
        if (i >= 0) {
            foo(a[i]);
        } else {
            oops(i);
        }
    }
    std::exit(2);
}

BOOST_NOINLINE void foo(int i) {
    bar(--i);
}

#include <iostream>
int main() {

    //[getting_started_catching_trace
    try {
        foo(5); // testing assert handler
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        const boost::stacktrace::stacktrace* st = boost::get_error_info<traced>(e);
        if (st) {
            std::cerr << *st << '\n'; /*<-*/ return 0; /*->*/
        } /*<-*/ return 3; /*->*/
    }
    //]

    return 5;
}

