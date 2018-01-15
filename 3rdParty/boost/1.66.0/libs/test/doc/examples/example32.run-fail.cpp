//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
#include <utility>


typedef std::pair<int,float> pair_type;

BOOST_TEST_DONT_PRINT_LOG_VALUE( pair_type )

BOOST_AUTO_TEST_CASE( test_list )
{
  pair_type p1( 2, 5.5f );
  pair_type p2( 2, 5.501f );

  BOOST_CHECK_EQUAL( p1, p2 );
}
//]
