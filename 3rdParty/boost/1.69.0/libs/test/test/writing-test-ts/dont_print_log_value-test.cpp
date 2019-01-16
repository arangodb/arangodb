//  (C) Copyright Marek Kurdej 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! BOOST_TEST_DONT_PRINT_LOG_VALUE unit test
// *****************************************************************************

#define BOOST_TEST_MODULE BOOST_TEST_DONT_PRINT_LOG_VALUE unit test
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <vector>

struct dummy_class {
    operator bool() const { return true; }

    bool operator==(dummy_class const&) const { return true;  }
    bool operator!=(dummy_class const&) const { return false; }
};

BOOST_TEST_DONT_PRINT_LOG_VALUE(dummy_class)

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE(single_object)
{
  dummy_class actual, expected;
  BOOST_TEST(actual == expected);
}

//____________________________________________________________________________//

// this one tests for contexts printing in dataset tests
std::vector<dummy_class> generate_vector()
{
  std::vector<dummy_class> out;
  out.push_back(dummy_class());
  out.push_back(dummy_class());
  out.push_back(dummy_class());
  return out;
}

//____________________________________________________________________________//

BOOST_DATA_TEST_CASE( test_data_case, boost::unit_test::data::make(generate_vector()))
{
  BOOST_TEST(sample);
}

// EOF
