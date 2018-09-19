// Copyright (C) 2015, Andrzej Krzemienski.
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

enum E1 {e1};
enum E2 {e2};

void test_converitng_assignment_of_different_enums()
{
  boost::optional<E2> o2(e2);
  boost::optional<E1> o1;
  o1 = o2;
}

int main() {}
