//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

// first file, should be exactly the same as second file except for the test name
BOOST_AUTO_TEST_SUITE(data)
BOOST_AUTO_TEST_SUITE(foo)

BOOST_AUTO_TEST_CASE(test1)
{
  BOOST_TEST(true);
}
BOOST_TEST_DECORATOR(* boost::unit_test::description("with description"))
BOOST_AUTO_TEST_CASE(test11)
{
  BOOST_TEST(true);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
