// Copyright (C) 2018, Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional.hpp"

// THIS TEST SHOULD FAIL TO COMPILE

void test_converitng_assignment_of_different_enums()
{
  const boost::optional<int> o1(1);
  const boost::optional<int> o2(2);
  swap(o1, o2); // no swap on const objects should compile
}

int main()
{
  test_converitng_assignment_of_different_enums();
}
