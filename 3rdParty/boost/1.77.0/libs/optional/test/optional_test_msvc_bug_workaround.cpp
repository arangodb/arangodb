// Copyright (C) 2015 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#define BOOST_OPTIONAL_CONFIG_NO_RVALUE_REFERENCES
#include "boost/optional/optional.hpp"

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"


struct Wrapper
{
  operator int () { return 9; }
  operator boost::optional<int> () { return 7; }
};

void test()
{
#if (!defined BOOST_NO_CXX11_RVALUE_REFERENCES)
  boost::optional<int> v = Wrapper();
  BOOST_TEST(v);
  BOOST_TEST_EQ(*v, 7);
#endif
} 

int main()
{
  test();
  return boost::report_errors();
}