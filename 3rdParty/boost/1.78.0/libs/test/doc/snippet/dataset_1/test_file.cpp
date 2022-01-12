//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org/libs/test for the library home page.
//
//  snippets included in the dataset documentation
// ***************************************************************************

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <boost/test/data/monomorphic.hpp>

#include <boost/test/data/monomorphic/generators/xrange.hpp>
#include <boost/test/data/monomorphic/zip.hpp>

// generation of a sequence/range
namespace data=boost::unit_test::data;


//[snippet_dataset1_1
BOOST_DATA_TEST_CASE( test_case_arity1_implicit, data::xrange(5) )
{
  BOOST_TEST((sample <= 4 && sample >= 0));
}
//]

//[snippet_dataset1_2
BOOST_DATA_TEST_CASE( test_case_arity1, data::xrange(5), my_var )
{
  BOOST_TEST((my_var <= 4 && my_var >= 0));
}
//]

//[snippet_dataset1_3
// The following definition of the dataset test case throws an exception before the
// test module starts (zip of non infinite or singleton datasets of different length)
BOOST_DATA_TEST_CASE( test_case_arity2, data::xrange(2) ^ data::xrange(5), apples, potatoes)
{
  BOOST_TEST((apples <= 1 && apples >= 0));
  BOOST_TEST((potatoes <= 4 && potatoes >= 0));
}
//]




//[snippet_dataset1_4
std::vector<int> generate()
{
  std::vector<int> out;
  out.push_back(3);
  out.push_back(1);
  out.push_back(7);
  return out;
}

const std::vector<int> v = generate();
BOOST_DATA_TEST_CASE( test_case_3, data::make(v), var1)
{
  BOOST_TEST_MESSAGE(var1);
  BOOST_CHECK(true);
}
//]


#include <vector>
#include <map>

std::vector<int> generate_vector()
{
  std::vector<int> out;
  out.push_back(3);
  out.push_back(1);
  out.push_back(7);
  return out;
}

typedef std::pair<const int, int> pair_int;
BOOST_TEST_DONT_PRINT_LOG_VALUE( pair_int )

const std::vector<int> v = generate_vector();
BOOST_DATA_TEST_CASE( test_case_1, data::make(v), var1)
{
  std::cout << var1 << std::endl;
  BOOST_TEST(true);
}


std::map<int, int> generate_map()
{
  std::vector<int> v = generate_vector();
  std::map<int, int> out;
  for(std::size_t i = 0; i < v.size(); i++)
  {
    out[v[i]] = (i * 7) % 19;
  }
  return out;
}

const std::map<int, int> m = generate_map();
BOOST_DATA_TEST_CASE( test_case_2, data::make(m), var1)
{
  std::cout << var1.first << " -- " << var1.second << std::endl;
  BOOST_TEST(true);
}
