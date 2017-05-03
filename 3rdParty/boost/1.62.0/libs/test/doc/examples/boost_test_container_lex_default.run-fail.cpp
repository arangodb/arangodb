//  (C) Copyright Raffi Enficiaud 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_container_lex_default
#include <boost/test/included/unit_test.hpp>
#include <vector>

namespace tt = boost::test_tools;

BOOST_TEST_SPECIALIZED_COLLECTION_COMPARE(std::vector<int>)

BOOST_AUTO_TEST_CASE( test_collections_vectors_lex )
{
  std::vector<int> a{1,2,3}, b{1,2,2};
  std::vector<long int> c{1,2,3,5}, d{1,2,3,4};

  BOOST_TEST(a < a); // extended diagnostic
  BOOST_TEST(a < b); // extended diagnostic
  BOOST_TEST(c < d); // no extended diagnostic
}
//]
