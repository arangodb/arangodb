//  (C) Copyright Gennadiy Rozental 2001-2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
// ***************************************************************************

#define BOOST_TEST_MODULE system call test example
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( broken_test )
{
  BOOST_CHECK_EQUAL( ::system("ls"), 0 );
}

