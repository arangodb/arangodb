// Copyright (C) 2018 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/core/lightweight_test.hpp"
#include "boost/optional/optional.hpp"

void test_assignment_to_empty()
{
  // this test used to fail on GCC 8.1.0/8.2.0/9.0.0 with -std=c++98
  boost::optional<int> oa, ob(1);
  BOOST_TEST(!oa);
  BOOST_TEST(ob);

  oa = ob;
  BOOST_TEST(oa);
}

int main()
{
  test_assignment_to_empty();
  return boost::report_errors();
}
