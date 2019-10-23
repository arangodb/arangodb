// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/throw_exception.hpp>
#include <boost/detail/lightweight_test.hpp>

class my_exception: public std::exception
{
};

class my_exception2: public std::exception, public boost::exception
{
};

int main()
{
    BOOST_TEST_THROWS( boost::throw_exception( my_exception() ), boost::exception );
    BOOST_TEST_THROWS( boost::throw_exception( my_exception2() ), boost::exception );

    BOOST_TEST_THROWS( BOOST_THROW_EXCEPTION( my_exception() ), boost::exception );
    BOOST_TEST_THROWS( BOOST_THROW_EXCEPTION( my_exception2() ), boost::exception );

    return boost::report_errors();
}
