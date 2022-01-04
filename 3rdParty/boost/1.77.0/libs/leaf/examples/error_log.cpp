// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This program demonstrates the use of leaf::on_error to print the path an
// error takes as it bubbles up the call stack. The printing code only runs if:
// - An error occurs, and
// - A handler that takes e_error_log argument is present. Otherwise none of the
//   error log machinery will be invoked by LEAF.

// This example is similar to error_trace, except the path the error takes is
// not captured, only printed.

#include <boost/leaf.hpp>
#include <iostream>
#include <cstdlib>

#define ENABLE_ERROR_LOG 1

namespace leaf = boost::leaf;

// The error log is activated only if an error-handling scope provides a handler
// for e_error_log.
struct e_error_log
{
    struct rec
    {
        char const * file;
        int line;
        friend std::ostream & operator<<( std::ostream & os, rec const & x )
        {
            return os << x.file << '(' << x.line << ')';
        }
    };

    e_error_log()
    {
        std::cerr << "Error! Log:" << std::endl;
    }

    // Our e_error_log instance is stateless, used only as a target to
    // operator<<.
    template <class T>
    friend std::ostream & operator<<( e_error_log const &, T const & x )
    {
        return std::cerr << x << std::endl;
    }
};

// The ERROR_LOG macro is designed for use in functions that detect or forward
// errors up the call stack. If an error occurs, and if an error-handling scope
// provides a handler for e_error_log, the supplied lambda is executed as the
// error bubbles up.
#define ERROR_LOG auto _log = leaf::on_error( []( e_error_log & log ) { log << e_error_log::rec{__FILE__, __LINE__}; } )

// Each function in the sequence below calls the previous function, and each
// function has failure_percent chance of failing. If a failure occurs, the
// ERROR_LOG macro will cause the path the error takes to be printed.
int const failure_percent = 25;

leaf::result<void> f1()
{
    ERROR_LOG;
    if( (std::rand()%100) > failure_percent )
        return { };
    else
        return leaf::new_error();
}

leaf::result<void> f2()
{
    ERROR_LOG;
    if( (std::rand()%100) > failure_percent )
        return f1();
    else
        return leaf::new_error();
}

leaf::result<void> f3()
{
    ERROR_LOG;
    if( (std::rand()%100) > failure_percent )
        return f2();
    else
        return leaf::new_error();
}

leaf::result<void> f4()
{
    ERROR_LOG;
    if( (std::rand()%100) > failure_percent )
        return f3();
    else
        return leaf::new_error();
}

leaf::result<void> f5()
{
    ERROR_LOG;
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
#if ENABLE_ERROR_LOG // This single #if enables or disables the printing of the error log.
            []( e_error_log const & )
            {
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
