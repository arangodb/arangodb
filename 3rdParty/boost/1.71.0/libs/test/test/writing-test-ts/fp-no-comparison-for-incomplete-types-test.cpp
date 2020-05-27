//  (C) Copyright Raffi Enficiaud 2019
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

// https://github.com/boostorg/test/issues/209
// no floating point comparison for abstract types.
struct abstract
{
    virtual ~abstract() = 0;
    bool operator==(const abstract &) const { return true; }
};

std::ostream &operator<<(std::ostream &ostr, const abstract &) {
    return ostr << "compared";
}

// the test is checking the compilation of the following snippet
void foo(abstract &a)
{
    // pre-C++11 compilers use a copy of the arguments in the
    // expansion of the BOOST_TEST macro
#if !defined(BOOST_TEST_MACRO_LIMITED_SUPPORT)
    BOOST_TEST(a == a);
    BOOST_TEST_CHECK(a == a);
    BOOST_TEST_REQUIRE(a == a);
    BOOST_TEST_WARN(a == a);
#endif
    BOOST_CHECK_EQUAL(a, a);
}

// we need at least one test
BOOST_AUTO_TEST_CASE(dummy)
{
    const int x = 1;
    BOOST_TEST(x == 1);
}
