// Copyright (C) 2014, andrzej Krzemienski.
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

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
//
// THIS TEST SHOULD FAIL TO COMPILE
//
void optional_reference__test_no_assign_from_Trefref()
{
  boost::optional<const int&> opt;
  opt = int(3);
}

#else
#  error "Test skipped. This cannot be implemented w/o rvalue references."
#endif
