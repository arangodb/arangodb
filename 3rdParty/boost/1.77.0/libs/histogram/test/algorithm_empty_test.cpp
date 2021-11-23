// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/weighted_mean.hpp>
#include <boost/histogram/algorithm/empty.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <unordered_map>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;
using boost::histogram::algorithm::empty;

template <typename Tag>
void run_tests() {
  auto ax = axis::integer<>(0, 10);

  {
    auto h = make(Tag(), ax);
    BOOST_TEST(empty(h, coverage::all));
    BOOST_TEST(empty(h, coverage::inner));
    for (int i = -1; i < 11; ++i) {
      h.reset();
      h(i);
      BOOST_TEST(!empty(h, coverage::all));
      if (i == -1 || i == 10) {
        BOOST_TEST(empty(h, coverage::inner));
      } else {
        BOOST_TEST(!empty(h, coverage::inner));
      }
    }
  }

  {
    auto h = make_s(Tag(), std::vector<accumulators::weighted_mean<>>(),
                    axis::integer<>(0, 10), axis::integer<>(0, 10));
    BOOST_TEST(empty(h, coverage::all));
    BOOST_TEST(empty(h, coverage::inner));
    h.reset();
    h(weight(2), -2, -4, sample(3));
    BOOST_TEST(!empty(h, coverage::all));
    BOOST_TEST(empty(h, coverage::inner));
    h.reset();
    h(weight(1), -4, 2, sample(2));
    BOOST_TEST(!empty(h, coverage::all));
    BOOST_TEST(empty(h, coverage::inner));
    h.reset();
    h(weight(3), 3, 5, sample(1));
    BOOST_TEST(!empty(h, coverage::all));
    BOOST_TEST(!empty(h, coverage::inner));
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
