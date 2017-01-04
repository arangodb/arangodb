// Copyright (C) 2014, Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com
//
#include "boost/optional.hpp"

//
// THIS TEST SHOULD FAIL TO COMPILE
//

#if !defined(BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS)

bool test_implicit_conversion_to_bool()
{
  boost::optional<int> opt;
  return opt;
}

#else
#  error "Test skipped: this compiler does not support explicit conversion operators."
#endif
