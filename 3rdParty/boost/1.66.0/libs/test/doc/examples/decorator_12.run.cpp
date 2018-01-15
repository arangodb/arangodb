//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_12
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;

struct Fx
{
  std::string s;
  Fx(std::string s = "") : s(s)
        { BOOST_TEST_MESSAGE("set up " << s); }
  ~Fx() { BOOST_TEST_MESSAGE("tear down " << s); }
};

void setup() { BOOST_TEST_MESSAGE("set up fun"); }
void teardown() { BOOST_TEST_MESSAGE("tear down fun"); }

BOOST_AUTO_TEST_SUITE(suite1,
  * utf::fixture<Fx>(std::string("FX"))
  * utf::fixture<Fx>(std::string("FX2")))

  BOOST_AUTO_TEST_CASE(test1, * utf::fixture(&setup, &teardown))
  {
    BOOST_TEST_MESSAGE("running test1");
    BOOST_TEST(true);
  }

  BOOST_AUTO_TEST_CASE(test2)
  {
    BOOST_TEST_MESSAGE("running test2");
    BOOST_TEST(true);
  }

BOOST_AUTO_TEST_SUITE_END()
//]
