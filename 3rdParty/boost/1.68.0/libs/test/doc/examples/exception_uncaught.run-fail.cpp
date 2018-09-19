//  (C) Copyright 2015 Boost.test team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

struct my_struct {
  my_struct(int var_) : var(var_)
  {
    if(var_ < 0) throw std::runtime_error("negative value not allowed");
  }
  int var;
};

BOOST_AUTO_TEST_CASE( test )
{
  my_struct instance(-2);
  // ...
}

BOOST_AUTO_TEST_CASE( test2 )
{
  BOOST_TEST(true);
}
//]
