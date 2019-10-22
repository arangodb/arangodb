// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/weighted_sum.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <boost/histogram/axis/integer.hpp>
#include "throw_exception.hpp"
#include <unordered_map>
#include <vector>
#include "utility_histogram.hpp"

using namespace boost::histogram;
using boost::histogram::algorithm::sum;

template <typename Tag>
void run_tests() {
  auto ax = axis::integer<>(0, 100);

  auto h1 = make(Tag(), ax);
  for (unsigned i = 0; i < 100; ++i) h1(i);
  BOOST_TEST_EQ(sum(h1), 100);

  auto h2 = make_s(Tag(), std::vector<double>(), ax, ax);
  for (unsigned i = 0; i < 100; ++i)
    for (unsigned j = 0; j < 100; ++j) h2(i, j);
  BOOST_TEST_EQ(sum(h2), 10000);

  auto h3 = make_s(Tag(), std::array<int, 102>(), ax);
  for (unsigned i = 0; i < 100; ++i) h3(i);
  BOOST_TEST_EQ(sum(h3), 100);

  auto h4 = make_s(Tag(), std::unordered_map<std::size_t, int>(), ax);
  for (unsigned i = 0; i < 100; ++i) h4(i);
  BOOST_TEST_EQ(sum(h4), 100);

  auto h5 =
      make_s(Tag(), std::vector<accumulators::weighted_sum<>>(), axis::integer<>(0, 1),
             axis::integer<int, axis::null_type, axis::option::none_t>(2, 4));
  h5(weight(2), 0, 2);
  h5(-1, 2);
  h5(1, 3);

  const auto v = algorithm::sum(h5);
  BOOST_TEST_EQ(v.value(), 4);
  BOOST_TEST_EQ(v.variance(), 6);
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
