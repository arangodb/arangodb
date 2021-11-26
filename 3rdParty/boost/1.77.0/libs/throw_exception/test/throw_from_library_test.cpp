// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
#endif

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wunknown-pragmas"
# pragma clang diagnostic ignored "-Wunknown-warning-option"
# pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
# pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
# pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include "lib1_throw.hpp"
#include "lib2_throw.hpp"
#include "lib3_throw.hpp"
#include <boost/exception/exception.hpp>
#include <boost/exception_ptr.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/core/lightweight_test.hpp>

void test_catch_by_type()
{
    BOOST_TEST_THROWS( lib1::f(), lib1::exception );
    BOOST_TEST_THROWS( lib2::f(), lib2::exception );
    BOOST_TEST_THROWS( lib3::f(), lib3::exception );
}

void test_catch_by_exception()
{
    BOOST_TEST_THROWS( lib2::f(), boost::exception );
    BOOST_TEST_THROWS( lib3::f(), boost::exception );
}

void test_exception_ptr()
{
    try
    {
        lib2::f();
    }
    catch( ... )
    {
        boost::exception_ptr p = boost::current_exception();

        BOOST_TEST_THROWS( boost::rethrow_exception( p ), lib2::exception );
        BOOST_TEST_THROWS( boost::rethrow_exception( p ), boost::exception );
    }

    try
    {
        lib3::f();
    }
    catch( ... )
    {
        boost::exception_ptr p = boost::current_exception();

        BOOST_TEST_THROWS( boost::rethrow_exception( p ), lib3::exception );
        BOOST_TEST_THROWS( boost::rethrow_exception( p ), boost::exception );
    }
}

void test_throw_line()
{
    try
    {
        lib3::f();
    }
    catch( boost::exception const & x )
    {
        int const * line = boost::get_error_info<boost::throw_line>( x );

        BOOST_TEST( line != 0 );
        BOOST_TEST_EQ( *line, 13 );
    }
}

int main()
{
    test_catch_by_type();
    test_catch_by_exception();
    test_exception_ptr();
    test_throw_line();

    return boost::report_errors();
}
