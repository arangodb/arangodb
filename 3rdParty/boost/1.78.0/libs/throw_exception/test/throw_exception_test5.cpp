// Copyright 2018, 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wunknown-pragmas"
# pragma clang diagnostic ignored "-Wunknown-warning-option"
# pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
# pragma clang diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
#endif

#include <boost/throw_exception.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/exception/info.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <string>

typedef boost::error_info<struct tag_error_code, int> error_code;
typedef boost::error_info<struct tag_error_string, std::string> error_string;

class my_exception: public std::exception, public boost::exception
{
};

class my_exception2: public std::exception, public virtual boost::exception
{
};

int main()
{
    try
    {
        boost::throw_exception( my_exception() << error_code( 123 ) << error_string( "error%%string" ) );
    }
    catch( boost::exception const & x )
    {
        {
            int const * code = boost::get_error_info<error_code>( x );

            BOOST_TEST( code != 0 );
            BOOST_TEST_EQ( *code, 123 );
        }

        {
            std::string const * str = boost::get_error_info<error_string>( x );

            BOOST_TEST( str != 0 );
            BOOST_TEST_EQ( *str, "error%%string" );
        }
    }

    try
    {
        BOOST_THROW_EXCEPTION( my_exception() << error_code( 123 ) << error_string( "error%%string" ) );
    }
    catch( boost::exception const & x )
    {
        {
            int const * code = boost::get_error_info<error_code>( x );

            BOOST_TEST( code != 0 );
            BOOST_TEST_EQ( *code, 123 );
        }

        {
            std::string const * str = boost::get_error_info<error_string>( x );

            BOOST_TEST( str != 0 );
            BOOST_TEST_EQ( *str, "error%%string" );
        }
    }

    try
    {
        boost::throw_exception( my_exception2() << error_code( 123 ) << error_string( "error%%string" ) );
    }
    catch( boost::exception const & x )
    {
        {
            int const * code = boost::get_error_info<error_code>( x );

            BOOST_TEST( code != 0 );
            BOOST_TEST_EQ( *code, 123 );
        }

        {
            std::string const * str = boost::get_error_info<error_string>( x );

            BOOST_TEST( str != 0 );
            BOOST_TEST_EQ( *str, "error%%string" );
        }
    }

    try
    {
        BOOST_THROW_EXCEPTION( my_exception2() << error_code( 123 ) << error_string( "error%%string" ) );
    }
    catch( boost::exception const & x )
    {
        {
            int const * code = boost::get_error_info<error_code>( x );

            BOOST_TEST( code != 0 );
            BOOST_TEST_EQ( *code, 123 );
        }

        {
            std::string const * str = boost::get_error_info<error_string>( x );

            BOOST_TEST( str != 0 );
            BOOST_TEST_EQ( *str, "error%%string" );
        }
    }

    return boost::report_errors();
}
