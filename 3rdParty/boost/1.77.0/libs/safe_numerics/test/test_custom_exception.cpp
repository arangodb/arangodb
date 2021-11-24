//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test custom exception

#include <iostream>
#include <functional>
#include <boost/safe_numerics/safe_integer_range.hpp>
#include <boost/safe_numerics/interval.hpp> // print variable range
#include <boost/safe_numerics/native.hpp> // native promotion policy
#include <boost/safe_numerics/exception.hpp> // safe_numerics_error
#include <boost/safe_numerics/exception_policies.hpp>

// log an exception condition but continue processing as though nothing has happened
// this would emulate the behavior of an unsafe type.
struct log_runtime_exception {
    log_runtime_exception() = default;
    void operator () (
        const boost::safe_numerics::safe_numerics_error & e,
        const char * message
    ){
        std::cout
            << "Caught system_error with code "
            << boost::safe_numerics::literal_string(e)
            << " and message " << message << '\n';
    }
};

// logging policy
// log arithmetic errors but ignore them and continue to execute
// implementation defined and undefined behavior is just executed
// without logging.

using logging_exception_policy = boost::safe_numerics::exception_policy<
    log_runtime_exception,  // arithmetic error
    log_runtime_exception,  // implementation defined behavior
    log_runtime_exception,  // undefined behavior
    log_runtime_exception   // uninitialized value
>;

template<unsigned int Min, unsigned int Max>
using sur = boost::safe_numerics::safe_unsigned_range<
    Min, // min value in range
    Max, // max value in range
    boost::safe_numerics::native, // promotion policy
    logging_exception_policy // exception policy
>;

template<int Min, int Max>
using ssr = boost::safe_numerics::safe_signed_range<
    Min, // min value in range
    Max, // max value in range
    boost::safe_numerics::native, // promotion policy
    logging_exception_policy // exception policy
>;

int main() {
    const sur<1910, 2099> test0 = 7; // note logged exception - value undefined
    std::cout << "test0 = " << test0 << '\n';
    const ssr<1910, 2099> test1 = 7; // note logged exception - value undefined
    std::cout << "test1 = " << test1 << '\n';
    const sur<0, 2> test2 = 7;       // note logged exception - value undefined
    std::cout << "test2 = " << test2 << '\n';
    const sur<1910, 2099> test3;     // unitialized value
    std::cout << "test3 = " << test3 << '\n';
    sur<1910, 2099> test4 = 2000;   // OK value
    std::cout << "test4 = " << test4 << boost::safe_numerics::make_interval(test4) << '\n';
    return 0;
}
