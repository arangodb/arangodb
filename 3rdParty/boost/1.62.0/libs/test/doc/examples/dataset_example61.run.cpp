//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE dataset_example61
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

namespace data = boost::unit_test::data;

int samples1[] = {1,2};
char const* samples2[] = {"qwerty", "asdfg"};

BOOST_DATA_TEST_CASE( 
      test1, 
      data::make(samples1)^samples2, 
      integer_values, 
      string_value)
{
  std::cout << integer_values << ", " << string_value << std::endl;
}
//]
