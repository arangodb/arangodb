// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/integer.hpp>
#include "throw_exception.hpp"
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/numeric.hpp>

using namespace boost::histogram;
using namespace boost::adaptors;

int main() {
  auto h = make_histogram(axis::integer<>(1, 4));
  h(1, weight(1));
  h(2, weight(2));
  h(3, weight(3));
  h(4, weight(4));

  auto s1 = boost::accumulate(h | filtered([](double x) { return x > 2; }), 0.0);
  BOOST_TEST_EQ(s1, 7);

  return boost::report_errors();
}
