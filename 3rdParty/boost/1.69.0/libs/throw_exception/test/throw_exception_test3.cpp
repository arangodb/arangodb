// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/throw_exception.hpp>
#include <boost/exception_ptr.hpp>
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
        boost::throw_exception( my_exception() );
    }
    catch( ... )
    {
        boost::exception_ptr p = boost::current_exception();

        BOOST_TEST_THROWS( boost::rethrow_exception( p ), my_exception );
        BOOST_TEST_THROWS( boost::rethrow_exception( p ), boost::exception );
    }

    try
    {
        boost::throw_exception( my_exception2() );
    }
    catch( ... )
    {
        boost::exception_ptr p = boost::current_exception();

        BOOST_TEST_THROWS( boost::rethrow_exception( p ), my_exception2 );
        BOOST_TEST_THROWS( boost::rethrow_exception( p ), boost::exception );
    }

    try
    {
        BOOST_THROW_EXCEPTION( my_exception() );
    }
    catch( ... )
    {
        boost::exception_ptr p = boost::current_exception();

        BOOST_TEST_THROWS( boost::rethrow_exception( p ), my_exception );
        BOOST_TEST_THROWS( boost::rethrow_exception( p ), boost::exception );
    }

    try
    {
        BOOST_THROW_EXCEPTION( my_exception2() );
    }
    catch( ... )
    {
        boost::exception_ptr p = boost::current_exception();

        BOOST_TEST_THROWS( boost::rethrow_exception( p ), my_exception2 );
        BOOST_TEST_THROWS( boost::rethrow_exception( p ), boost::exception );
    }

    return boost::report_errors();
}
