// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/argument_traits.hpp>
#include <boost/histogram/sample.hpp>
#include <boost/histogram/weight.hpp>

namespace dtl = boost::histogram::detail;

int main() {
  using boost::histogram::sample;
  using boost::histogram::weight;

  // is_weight
  {
    struct A {};
    using B = int;
    BOOST_TEST_NOT(dtl::is_weight<A>::value);
    BOOST_TEST_NOT(dtl::is_weight<B>::value);
    BOOST_TEST_NOT(dtl::is_weight<decltype(sample(0))>::value);
    BOOST_TEST(dtl::is_weight<decltype(weight(0))>::value);
  }

  // is_sample
  {
    struct A {};
    using B = int;
    BOOST_TEST_NOT(dtl::is_sample<A>::value);
    BOOST_TEST_NOT(dtl::is_sample<B>::value);
    BOOST_TEST_NOT(dtl::is_sample<decltype(weight(0))>::value);
    BOOST_TEST(dtl::is_sample<decltype(sample(0))>::value);
    BOOST_TEST(dtl::is_sample<decltype(sample(0, A{}))>::value);
  }

  using T1 = dtl::argument_traits<int>;
  using E1 = dtl::argument_traits_holder<1, 0, -1, -1, std::tuple<>>;
  BOOST_TEST_TRAIT_SAME(T1, E1);

  using T2 = dtl::argument_traits<int, int>;
  using E2 = dtl::argument_traits_holder<2, 0, -1, -1, std::tuple<>>;
  BOOST_TEST_TRAIT_SAME(T2, E2);

  using T3 = dtl::argument_traits<decltype(weight(0)), int, int>;
  using E3 = dtl::argument_traits_holder<2, 1, 0, -1, std::tuple<>>;
  BOOST_TEST_TRAIT_SAME(T3, E3);

  using T4 = dtl::argument_traits<decltype(weight(0)), int, int, decltype(sample(0))>;
  using E4 = dtl::argument_traits_holder<2, 1, 0, 3, std::tuple<int>>;
  BOOST_TEST_TRAIT_SAME(T4, E4);

  using T5 = dtl::argument_traits<int, decltype(sample(0, 0.0))>;
  using E5 = dtl::argument_traits_holder<1, 0, -1, 1, std::tuple<int, double>>;
  BOOST_TEST_TRAIT_SAME(T5, E5);

  return boost::report_errors();
}
