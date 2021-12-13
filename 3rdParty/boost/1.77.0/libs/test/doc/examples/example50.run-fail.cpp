//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
#include <fstream>

struct MyConfig {
  MyConfig() : test_log( "example.log" ) {
    boost::unit_test::unit_test_log.set_stream( test_log );
  }
  ~MyConfig() {
    boost::unit_test::unit_test_log.set_stream( std::cout );
  }
  std::ofstream test_log;
};

BOOST_TEST_GLOBAL_CONFIGURATION( MyConfig );

BOOST_AUTO_TEST_CASE( test_case ) {
  BOOST_TEST( false );
}
//]
