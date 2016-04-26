//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

bool is_even( int i )
{
  return i%2 == 0;
}

BOOST_AUTO_TEST_CASE( test_is_even )
{
  BOOST_CHECK_PREDICATE( is_even, (14) );

  int i = 17;
  BOOST_CHECK_PREDICATE( is_even, (i) );
}
//]
