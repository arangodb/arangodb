//  (C) Copyright Gennadiy Rozental 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : testing BOOST_TEST_UNDER_DEBUGGER compilation and operation
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_TOOLS_DEBUGGABLE
#define BOOST_TEST_MODULE tools under debugger test
#include <boost/test/unit_test.hpp>

//____________________________________________________________________________//

static int
foo( int arg )
{
  if( arg == 0 )
    throw std::runtime_error("Oops");

  return arg * arg;
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test )
{
    int i = 2;
    BOOST_TEST( foo(i)+1 == 5 );

    BOOST_TEST( foo(i)+1 == 5, "My message" );

    BOOST_CHECK_THROW( foo(0), std::runtime_error );
}

// EOF
