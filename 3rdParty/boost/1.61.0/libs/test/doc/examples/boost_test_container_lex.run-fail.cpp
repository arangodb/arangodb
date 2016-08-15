//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_container_lex
#include <boost/test/included/unit_test.hpp>
#include <vector>

namespace tt = boost::test_tools;

BOOST_AUTO_TEST_CASE( test_collections_vectors_lex )
{
  std::vector<int> a{1,2,3}, b{1,2,2}, c{1,2,3,4};
  
  BOOST_TEST(a < a, tt::lexicographic());
  BOOST_TEST(a < b, tt::lexicographic());
  BOOST_TEST(a < c, tt::lexicographic());
  BOOST_TEST(a >= c, tt::lexicographic());
  
  // does not compile
  //BOOST_TEST(a == c, tt::lexicographic());
  //BOOST_TEST(a != c, tt::lexicographic());
}
//]
