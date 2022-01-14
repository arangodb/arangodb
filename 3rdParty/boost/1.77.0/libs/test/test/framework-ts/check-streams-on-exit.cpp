//  (C) Copyright Raffi Enficiaud 2018.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page

#define BOOST_TEST_MODULE release_streams
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>
#include <iostream>
namespace utf = boost::unit_test;

// to avoid empty test bed
BOOST_AUTO_TEST_CASE(test1)
{
  BOOST_TEST(true);
}

int main(int argc, char* argv[])
{
  int return_value = utf::unit_test_main(init_unit_test, argc, argv);
  if(return_value != 0) {
      std::cerr << "Error detected while running the test bed" << std::endl;
      return return_value;
  }

  if( &utf::results_reporter::get_stream() != &std::cerr ) {
      std::cerr << "report stream not released properly" << std::endl;
      return 1;
  }

  utf::output_format outputs_to_tests[] = { utf::OF_CLF, utf::OF_XML, utf::OF_JUNIT, utf::OF_CUSTOM_LOGGER};
  const char* const outputs_to_test_names[] = {"CLF", "XML", "JUNIT", "CUSTOM"};

  for(int i = 0; i < sizeof(outputs_to_tests)/sizeof(outputs_to_tests[0]); i++) {
      std::ostream* stream = utf::unit_test_log.get_stream(outputs_to_tests[i]);
      if( stream && stream != &std::cout ) {
          std::cerr << "logger stream not released properly for format '" << outputs_to_test_names[i] << "'" << std::endl;
          return 1;
      }
  }
  return 0;
}
