//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

class my_exception{};

void some_func( int i ) { if( i<0 ) throw my_exception(); }

BOOST_AUTO_TEST_CASE( test )
{
    BOOST_CHECK_NO_THROW( some_func(-1) );
    BOOST_CHECK_NO_THROW(
      do {
        int i(-2);
        some_func(i);
      } while(0)
    );
}
//]
