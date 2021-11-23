//  (C) Copyright Raffi Enficiaud 2016.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

// explicitely not using BOOST_TEST_MODULE to check the compilation of the
// parser (prints the BOOST_TEST_MODULE if defined)
// Also this should be the included version

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( check )
{
  BOOST_TEST( true );
}
