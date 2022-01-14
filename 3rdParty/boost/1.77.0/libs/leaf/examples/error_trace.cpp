// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This program demonstrates the use of leaf::on_error to capture the path an
// error takes as is bubbles up the call stack. The error path-capturing code
// only runs if:
// - An error occurs, and
// - A handler that takes e_error_trace argument is present. Otherwise none of
//   the error trace machinery will be invoked by LEAF.

// This example is similar to error_log, except the path the error takes is
// recorded in a std::deque, rather than just printed in-place.

#include <boost/leaf.hpp>
#include <iostream>
#include <deque>
#include <cstdlib>

#define ENABLE_ERROR_TRACE 1

namespace leaf = boost::leaf;

// The error trace is activated only if an error-handling scope provides a
// handler for e_error_trace.
struct e_error_trace
{
    struct rec
    {
        char const * file;
        int line;
        friend std::ostream & operator<<( std::ostream & os, rec const & x )
        {
            return os << x.file << '(' << x.line << ')' << std::endl;
        }
    };

    std::deque<rec> value;

    friend std::ostream & operator<<( std::ostream & os, e_error_trace const & tr )
    {
        for( auto & i : tr.value )
            os << i;
        return os;
    }
};

// The ERROR_TRACE macro is designed for use in functions that detect or forward
// errors up the call stack. If an error occurs, and if an error-handling scope
// provides a handler for e_error_trace, the supplied lambda is executed as the
// error bubbles up.
#define ERROR_TRACE auto _trace = leaf::on_error( []( e_error_trace & tr ) { tr.value.emplace_front(e_error_trace::rec{__FILE__, __LINE__}); } )

// Each function in the sequence below calls the previous function, and each
// function has failure_percent chance of failing. If a failure occurs, the
// ERROR_TRACE macro will cause the path the error takes to be captured in an
// e_error_trace.
int const failure_percent = 25;

leaf::result<void> f1()
{
    ERROR_TRACE;
    if( (std::rand()%100) > failure_percent )
        return { };
    else
        return leaf::new_error();
}

leaf::result<void> f2()
{
    ERROR_TRACE;
    if( (std::rand()%100) > failure_percent )
        return f1();
    else
        return leaf::new_error();
}

leaf::result<void> f3()
{
    ERROR_TRACE;
    if( (std::rand()%100) > failure_percent )
        return f2();
    else
        return leaf::new_error();
}

leaf::result<void> f4()
{
    ERROR_TRACE;
    if( (std::rand()%100) > failure_percent )
        return f3();
    else
        return leaf::new_error();
}

leaf::result<void> f5()
{
    ERROR_TRACE;
    if( (std::rand()%100) > failure_percent )
        return f4();
    else
        return leaf::new_error();
}

int main()
{
    for( int i=0; i!=10; ++i )
        leaf::try_handle_all(
            [&]() -> leaf::result<void>
            {
                std::cout << "Run # " << i << ": ";
                BOOST_LEAF_CHECK(f5());
                std::cout << "Success!" << std::endl;
                return { };
            },
#if ENABLE_ERROR_TRACE // This single #if enables or disables the capturing of the error trace.
            []( e_error_trace const & tr )
            {
                std::cerr << "Error! Trace:" << std::endl << tr;
            },
#endif
            []
            {
                std::cerr << "Error!" << std::endl;
            } );
    return 0;
}

////////////////////////////////////////

#ifdef BOOST_LEAF_NO_EXCEPTIONS

namespace boost
{
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e )
    {
        std::cerr << "Terminating due to a C++ exception under BOOST_LEAF_NO_EXCEPTIONS: " << e.what();
        std::terminate();
    }

    struct source_location;
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e, boost::source_location const & )
    {
        throw_exception(e);
    }
}

#endif
