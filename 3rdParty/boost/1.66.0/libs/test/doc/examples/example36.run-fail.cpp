//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test )
{
  int col1 [] = { 1, 2, 3, 4, 5, 6, 7 };
  int col2 [] = { 1, 2, 4, 4, 5, 7, 7 };

  BOOST_CHECK_EQUAL_COLLECTIONS( col1, col1+7, col2, col2+7 );
}
//]
