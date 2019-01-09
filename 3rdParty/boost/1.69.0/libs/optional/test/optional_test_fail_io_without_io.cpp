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

#include <iostream>
#include "boost/optional.hpp"
// but no boost/optional/optional_io.hpp

// THIS TEST SHOULD FAIL TO COMPILE
// Unless one includes header boost/optional/optional_io.hpp, it should not be possible
// to stream out an optional object.

void test_streaming_out_optional()
{
  boost::optional<int> opt;
  std::cout << opt;
}