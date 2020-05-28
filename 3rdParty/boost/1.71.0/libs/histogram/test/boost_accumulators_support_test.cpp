// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/integer.hpp>
#include "throw_exception.hpp"
#include <boost/histogram/make_histogram.hpp>
#include <boost/histogram/storage_adaptor.hpp>

namespace ba = boost::accumulators;

int main() {
  using namespace boost::histogram;

  // mean
  {
    using mean = ba::accumulator_set<double, ba::stats<ba::tag::mean>>;

    auto h = make_histogram_with(dense_storage<mean>(), axis::integer<>(0, 2));
    h(0, sample(1));
    h(0, sample(2));
    h(0, sample(3));
    h(1, sample(2));
    h(1, sample(3));
    BOOST_TEST_EQ(ba::count(h[0]), 3);
    BOOST_TEST_EQ(ba::mean(h[0]), 2);
    BOOST_TEST_EQ(ba::count(h[1]), 2);
    BOOST_TEST_EQ(ba::mean(h[1]), 2.5);
    BOOST_TEST_EQ(ba::count(h[2]), 0);

    auto h2 = h; // copy ok
    BOOST_TEST_EQ(ba::count(h2[0]), 3);
    BOOST_TEST_EQ(ba::mean(h2[0]), 2);
    BOOST_TEST_EQ(ba::count(h2[1]), 2);
    BOOST_TEST_EQ(ba::mean(h2[1]), 2.5);
    BOOST_TEST_EQ(ba::count(h2[2]), 0);
  }
  return boost::report_errors();
}
