// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/args_type.hpp>
#include <tuple>
#include "std_ostream.hpp"

using namespace boost::histogram::detail;

struct Foo {
  static int f1(char);
  int f2(long) const;
};

int main() {
  BOOST_TEST_TRAIT_SAME(args_type<decltype(&Foo::f1)>, std::tuple<char>);
  BOOST_TEST_TRAIT_SAME(args_type<decltype(&Foo::f2)>, std::tuple<long>);
  BOOST_TEST_TRAIT_SAME(arg_type<decltype(&Foo::f1)>, char);
  BOOST_TEST_TRAIT_SAME(arg_type<decltype(&Foo::f2)>, long);

  return boost::report_errors();
}
