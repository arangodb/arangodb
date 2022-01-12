// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include "throw_exception.hpp"

using namespace boost::histogram::detail;

int main() {
  using T = std::true_type;
  using F = std::false_type;

  // check that branch not taken does not have to compile
  BOOST_TEST_EQ(static_if<T>([](auto) { return 1; }, [] {}, 0), 1);
  BOOST_TEST_EQ(static_if<F>([] {}, [](auto) { return 1; }, 0), 1);

  BOOST_TEST_EQ(static_if_c<true>([](auto) { return 1; }, [] {}, 0), 1);
  BOOST_TEST_EQ(static_if_c<false>([] {}, [](auto) { return 1; }, 0), 1);

  // check that noexcept is correctly propagated
  auto may_throw = [](auto x) { return x; };
  auto no_throw = [](auto x) noexcept { return x; };

  // make this work with -fno-exceptions
  BOOST_TEST_EQ(noexcept(static_if<F>(no_throw, may_throw, 0)), noexcept(may_throw(0)));
  BOOST_TEST(noexcept(static_if<T>(no_throw, may_throw, 0)));

  return boost::report_errors();
}
