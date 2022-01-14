//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

extern void foo( int i );

BOOST_AUTO_TEST_CASE( test_external_interface )
{
  for( int i = 3; i >=0; i-- ) {
    BOOST_TEST_CHECKPOINT( "Calling 'foo' with i=" << i );
    foo( i );
  }
}

void goo( int value )
{
  BOOST_TEST_CHECKPOINT( "Inside goo with value '" << value << "'");
}

void foo( int i )
{
  if( i == 1 )
      throw std::runtime_error("Undefined Behaviour ahead!");
  // following line may not raise an exception on some compilers:
  // Undefined Behaviour is implementation specific
  goo( 2/(i-1) );
}
//]
