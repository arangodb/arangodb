//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_sequence_per_element
#include <boost/test/included/unit_test.hpp>
#include <vector>
#include <list>
namespace tt = boost::test_tools;

BOOST_AUTO_TEST_CASE( test_sequence_per_element )
{
  std::vector<int> a{1,2,3};
  std::vector<long> b{1,5,3};
  std::list<short> c{1,5,3,4};
  
  BOOST_TEST(a == b, tt::per_element()); // nok: a[1] != b[1]
  
  BOOST_TEST(a != b, tt::per_element()); // nok: a[0] == b[0] ...
  BOOST_TEST(a <= b, tt::per_element()); // ok
  BOOST_TEST(b  < c, tt::per_element()); // nok: size mismatch
  BOOST_TEST(b >= c, tt::per_element()); // nok: size mismatch
  BOOST_TEST(b != c, tt::per_element()); // nok: size mismatch
}
//]
