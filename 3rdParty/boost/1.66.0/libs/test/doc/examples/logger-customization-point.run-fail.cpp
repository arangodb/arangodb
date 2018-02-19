//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! Customization point for printing user defined types
// *****************************************************************************

//[example_code
#define BOOST_TEST_MODULE logger-customization-point
#include <boost/test/included/unit_test.hpp>

namespace user_defined_namespace {
  struct user_defined_type {
      int value;

      user_defined_type(int value_) : value(value_)
      {}

      bool operator==(int right) const {
          return right == value;
      }
  };
}

namespace user_defined_namespace {
  std::ostream& boost_test_print_type(std::ostream& ostr, user_defined_type const& right) {
      ostr << "** value of user_defined_type is " << right.value << " **";
      return ostr;
  }
}

BOOST_AUTO_TEST_CASE(test1)
{
    user_defined_namespace::user_defined_type t(10);
    BOOST_TEST(t == 11);

    using namespace user_defined_namespace;
    user_defined_type t2(11);
    BOOST_TEST(t2 == 11);
}
//]
