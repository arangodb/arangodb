// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/accumulators/weighted_sum.hpp>
#include <boost/histogram/detail/cat.hpp>
#include <boost/histogram/detail/common_type.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include "std_ostream.hpp"

using namespace boost::histogram;
using namespace boost::histogram::literals;

int main() {
  BOOST_TEST_EQ(detail::cat("foo", 1, "bar"), "foo1bar");

  // literals
  {
    BOOST_TEST_TRAIT_SAME(std::integral_constant<unsigned, 0>, decltype(0_c));
    BOOST_TEST_TRAIT_SAME(std::integral_constant<unsigned, 3>, decltype(3_c));
    BOOST_TEST_EQ(decltype(10_c)::value, 10);
    BOOST_TEST_EQ(decltype(213_c)::value, 213);
  }

  // common_storage
  {
    BOOST_TEST_TRAIT_SAME(
        detail::common_storage<unlimited_storage<>, unlimited_storage<>>,
        unlimited_storage<>);
    BOOST_TEST_TRAIT_SAME(
        detail::common_storage<dense_storage<double>, dense_storage<double>>,
        dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(
        detail::common_storage<dense_storage<int>, dense_storage<double>>,
        dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(
        detail::common_storage<dense_storage<double>, dense_storage<int>>,
        dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(
        detail::common_storage<dense_storage<double>, unlimited_storage<>>,
        dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(detail::common_storage<dense_storage<int>, unlimited_storage<>>,
                          unlimited_storage<>);
    BOOST_TEST_TRAIT_SAME(detail::common_storage<dense_storage<double>, weight_storage>,
                          weight_storage);
  }

  return boost::report_errors();
}
