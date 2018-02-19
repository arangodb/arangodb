//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE fixture_04
#include <boost/test/included/unit_test.hpp>

struct MyGlobalFixture {
  MyGlobalFixture() {
      BOOST_TEST_MESSAGE( "ctor fixture i=" << i );
  }
  void setup() {
      BOOST_TEST_MESSAGE( "setup fixture i=" << i );
      i++;
  }
  void teardown() {
      BOOST_TEST_MESSAGE( "teardown fixture i=" << i );
      i += 2;
  }
  ~MyGlobalFixture() {
      BOOST_TEST_MESSAGE( "dtor fixture i=" << i );
  }
  static int i;
};
int MyGlobalFixture::i = 0;

BOOST_TEST_GLOBAL_FIXTURE( MyGlobalFixture );

BOOST_AUTO_TEST_CASE(test_case1)
{
  BOOST_TEST_MESSAGE("running test_case1");
  BOOST_TEST(MyGlobalFixture::i == 1);
}

BOOST_AUTO_TEST_CASE(test_case2)
{
  BOOST_TEST_MESSAGE("running test_case2");
  BOOST_TEST(MyGlobalFixture::i == 3);
}
//]
