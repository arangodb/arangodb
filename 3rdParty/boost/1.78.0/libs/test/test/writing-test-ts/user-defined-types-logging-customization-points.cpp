//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! Customization point for printing user defined types
// *****************************************************************************

#define BOOST_TEST_MODULE user type logger customization points
#include <boost/test/unit_test.hpp>

namespace printing_test {
struct user_defined_type {
    int value;

    user_defined_type(int value_) : value(value_)
    {}

    bool operator==(int right) const {
        return right == value;
    }
};

std::ostream& boost_test_print_type(std::ostream& ostr, user_defined_type const& right) {
    ostr << "** value of my type is " << right.value << " **";
    return ostr;
}
}

//using namespace printing_test;

BOOST_AUTO_TEST_CASE(test1)
{
    //using printing_test::user_defined_type;
    printing_test::user_defined_type t(10);
    BOOST_CHECK_EQUAL(t, 10);
#ifndef BOOST_TEST_MACRO_LIMITED_SUPPORT
    BOOST_TEST(t == 10);
#endif
}

// on unary expressions as well
struct s {
  operator bool() const { return true; }
};
std::ostream &boost_test_print_type(std::ostream &o, const s &) {
  return o << "printed-s";
}

BOOST_AUTO_TEST_CASE( test_logs )
{
    BOOST_TEST(s());
}
