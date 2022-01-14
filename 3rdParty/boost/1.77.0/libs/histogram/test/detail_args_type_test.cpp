// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/args_type.hpp>
#include <tuple>
#include "std_ostream.hpp"

namespace dtl = boost::histogram::detail;

struct Foo {
  int f1(char);
  int f2(long) const;
  static int f3(char, int);
  auto f4(char, int) { return 0; };
};

int main() {
  BOOST_TEST_TRAIT_SAME(dtl::args_type<decltype(&Foo::f1)>, std::tuple<char>);
  BOOST_TEST_TRAIT_SAME(dtl::args_type<decltype(&Foo::f2)>, std::tuple<long>);
  BOOST_TEST_TRAIT_SAME(dtl::args_type<decltype(&Foo::f3)>, std::tuple<char, int>);
  BOOST_TEST_TRAIT_SAME(dtl::args_type<decltype(&Foo::f4)>, std::tuple<char, int>);

  BOOST_TEST_TRAIT_SAME(dtl::arg_type<decltype(&Foo::f3)>, char);
  BOOST_TEST_TRAIT_SAME(dtl::arg_type<decltype(&Foo::f3), 1>, int);

  return boost::report_errors();
}
