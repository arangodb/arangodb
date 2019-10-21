// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional/optional.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"
#include "boost/none.hpp"


struct SemiRegular // no operator==
{ 
private: void operator==(SemiRegular const&) const {}
private: void operator!=(SemiRegular const&) const {}
};

void test_equal_to_none_of_noncomparable_T()
{
  boost::optional<SemiRegular> i = SemiRegular();
  boost::optional<SemiRegular> o;
  
  BOOST_TEST(i != boost::none);
  BOOST_TEST(boost::none != i);
  BOOST_TEST(o == boost::none);
  BOOST_TEST(boost::none == o);
}

int main()
{
  test_equal_to_none_of_noncomparable_T();
  return boost::report_errors();
}
