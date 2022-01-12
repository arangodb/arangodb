// Copyright (C) 2016 Andrzej Krzemienski.
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

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"

//#ifndef BOOST_OPTIONAL_NO_CONVERTING_ASSIGNMENT
//#ifndef BOOST_OPTIONAL_NO_CONVERTING_COPY_CTOR

using boost::optional;

struct Value
{
  explicit Value(int) {}
};

#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
template <typename T>
void test_brace_init()
{
  optional<T> o = {};
  BOOST_TEST(!o);
}

template <typename T>
void test_brace_assign()
{
  optional<T> o;
  o = {};
  BOOST_TEST(!o);
} 
#endif

int main()
{
#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX
  test_brace_init<int>();
  test_brace_init<Value>();
  test_brace_assign<int>();
  test_brace_assign<Value>();
#endif

  return boost::report_errors();
}
