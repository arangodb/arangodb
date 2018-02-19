//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.


//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

bool moo( int arg1, int arg2, int mod ) { return ((arg1+arg2) % mod) == 0; }

BOOST_AUTO_TEST_CASE( test )
{
  int i = 17;
  int j = 15;
  unit_test_log.set_threshold_level( log_warnings );
  BOOST_WARN( moo( 12,i,j ) );
  BOOST_WARN_PREDICATE( moo, (12)(i)(j) );
}
//]
