//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

#if defined(BOOST_MSVC) && (BOOST_MSVC > 1900)
// VS2017+ compiler optimizations may cause this code NOT to crash.
#pragma optimize("", off)
#endif


void foo( int ) {}

#if defined(BOOST_MSVC) && (BOOST_MSVC > 1900)
#pragma optimize("", on)
#endif

BOOST_AUTO_TEST_CASE( test_case )
{
  int* p = 0;

  BOOST_TEST_PASSPOINT();
  ++p;

  BOOST_TEST_PASSPOINT();
  ++p;

  BOOST_TEST_PASSPOINT();
  foo( *p );
}
//]
