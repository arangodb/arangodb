//  Copyright (c) 2019 Raffi Enficiaud
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>
#include <functional>
#include <sstream>

using namespace boost::unit_test;

void test_function(int i) {
  BOOST_TEST(i >= 1);
}

// helper
int read_integer(const std::string &str) {
  std::istringstream buff( str );
  int number = 0;
  buff >> number;
  if(buff.fail()) {
    // it is also possible to raise a boost.test specific exception.
    throw framework::setup_error("Argument '" + str + "' not integer");
  }
  return number;
}

bool init_unit_test()
{
  int argc = boost::unit_test::framework::master_test_suite().argc;
  char** argv = boost::unit_test::framework::master_test_suite().argv;

  if( argc <= 1) {
      return false; // returning false to indicate an error
  }

  if( std::string(argv[1]) == "--create-parametrized" ) {
    if(argc < 3) {
      // the logging availability depends on the logger type
      BOOST_TEST_MESSAGE("Not enough parameters");
      return false;
    }

    int number_tests = read_integer(argv[2]);
    int test_start = 0;
    if(argc > 3) {
        test_start = read_integer(argv[3]);
    }

    for(int i = test_start; i < number_tests; i++) {
        std::ostringstream ostr;
        ostr << "name " << i;
        // create test-cases, avoiding duplicate names
        framework::master_test_suite().
            add( BOOST_TEST_CASE_NAME( std::bind(&test_function, i), ostr.str().c_str() ) );
    }
  }
  return true;
}
//]
