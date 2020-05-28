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

#include "boost/core/ignore_unused.hpp"
#include "boost/core/lightweight_test.hpp"
#include "boost/optional/detail/optional_config.hpp"

#if (defined BOOST_NO_CXX11_RVALUE_REFERENCES) || (defined BOOST_OPTIONAL_CONFIG_NO_LEGAL_CONVERT_FROM_REF)

int main() { return 0; }

#else

struct S {};

struct Binder
{
  S& ref_;
  template <typename R> Binder (R&&r) : ref_(r) {}
};

int main()
{
  S s ; 
  Binder b = s;
  boost::ignore_unused(b);
  return 0;
}

#endif
