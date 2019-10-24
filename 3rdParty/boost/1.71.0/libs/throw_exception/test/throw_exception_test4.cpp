// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/throw_exception.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/detail/lightweight_test.hpp>

class my_exception: public std::exception
{
};

class my_exception2: public std::exception, public boost::exception
{
};

int main()
{
    try
    {
        BOOST_THROW_EXCEPTION( my_exception() );
    }
    catch( boost::exception const & x )
    {
        int const * line = boost::get_error_info<boost::throw_line>( x );

        BOOST_TEST( line != 0 );
        BOOST_TEST_EQ( *line, 24 );
    }

    try
    {
        BOOST_THROW_EXCEPTION( my_exception2() );
    }
    catch( boost::exception const & x )
    {
        int const * line = boost::get_error_info<boost::throw_line>( x );

        BOOST_TEST( line != 0 );
        BOOST_TEST_EQ( *line, 36 );
    }

    return boost::report_errors();
}
