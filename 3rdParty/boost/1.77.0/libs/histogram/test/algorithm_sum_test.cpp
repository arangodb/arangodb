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
#include <unordered_map>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;
using boost::histogram::algorithm::sum;

template <typename Tag>
void run_tests() {
  auto ax = axis::integer<>(0, 10);

  {
    auto h = make(Tag(), ax);
    std::fill(h.begin(), h.end(), 1);
    BOOST_TEST_EQ(sum(h), 12);
    BOOST_TEST_EQ(sum(h, coverage::inner), 10);
  }

  {
    auto h = make_s(Tag(), std::array<int, 12>(), ax);
    std::fill(h.begin(), h.end(), 1);
    BOOST_TEST_EQ(sum(h), 12);
    BOOST_TEST_EQ(sum(h, coverage::inner), 10);
  }

  {
    auto h = make_s(Tag(), std::unordered_map<std::size_t, int>(), ax);
    std::fill(h.begin(), h.end(), 1);
    BOOST_TEST_EQ(sum(h), 12);
    BOOST_TEST_EQ(sum(h, coverage::inner), 10);
  }

  {
    auto h = make_s(Tag(), std::vector<double>(), ax, ax);
    std::fill(h.begin(), h.end(), 1);
    BOOST_TEST_EQ(sum(h), 12 * 12);
    BOOST_TEST_EQ(sum(h, coverage::inner), 10 * 10);
  }

  {
    using W = accumulators::weighted_sum<>;
    auto h = make_s(Tag(), std::vector<W>(), axis::integer<>(0, 2),
                    axis::integer<int, axis::null_type, axis::option::none_t>(2, 4));
    W w(0, 2);
    for (auto&& x : h) {
      x = w;
      w = W(w.value() + 1, 2);
    }

    // x-axis has 4 bins, y-axis has 2 = 8 bins total with 4 inner bins

    const auto v1 = algorithm::sum(h);
    BOOST_TEST_EQ(v1.value(), 0 + 1 + 2 + 3 + 4 + 5 + 6 + 7);
    BOOST_TEST_EQ(v1.variance(), 8 * 2);

    const auto v2 = algorithm::sum(h, coverage::inner);
    BOOST_TEST_EQ(v2.value(), 1 + 2 + 5 + 6);
    BOOST_TEST_EQ(v2.variance(), 4 * 2);
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
