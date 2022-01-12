// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <boost/histogram/accumulators/sum.hpp>
#include "throw_exception.hpp"
#include "utility_str.hpp"

using namespace boost::histogram;
using namespace std::literals;

int main() {
  double bad_sum = 0;
  bad_sum += 1;
  bad_sum += 1e100;
  bad_sum += 1;
  bad_sum += -1e100;
  BOOST_TEST_EQ(bad_sum, 0); // instead of 2

  using s_t = accumulators::sum<double>;
  s_t sum;
  ++sum;
  BOOST_TEST_EQ(sum, 1);
  BOOST_TEST_EQ(sum.value(), 1);
  BOOST_TEST_EQ(sum.large_part(), 1);
  BOOST_TEST_EQ(sum.small_part(), 0);
  BOOST_TEST_EQ(str(sum), "sum(1 + 0)"s);
  BOOST_TEST_EQ(str(sum, 15, false), "     sum(1 + 0)"s);
  BOOST_TEST_EQ(str(sum, 15, true), "sum(1 + 0)     "s);

#include <boost/histogram/detail/ignore_deprecation_warning_begin.hpp>

  BOOST_TEST_EQ(sum.large(), 1);
  BOOST_TEST_EQ(sum.small(), 0);

#include <boost/histogram/detail/ignore_deprecation_warning_end.hpp>

  sum += 1e100;
  BOOST_TEST_EQ(sum, (s_t{1e100, 1}));
  ++sum;
  BOOST_TEST_EQ(sum, (s_t{1e100, 2}));
  sum += -1e100;
  BOOST_TEST_EQ(sum, (s_t{0, 2}));
  BOOST_TEST_EQ(sum, 2); // correct answer
  BOOST_TEST_EQ(sum.value(), 2);
  BOOST_TEST_EQ(sum.large_part(), 0);
  BOOST_TEST_EQ(sum.small_part(), 2);

  sum = s_t{1e100, 1};
  sum += s_t{1e100, 1};
  BOOST_TEST_EQ(sum, (s_t{2e100, 2}));
  sum = s_t{1e100, 1};
  sum += s_t{1, 0};
  BOOST_TEST_EQ(sum, (s_t{1e100, 2}));
  sum = s_t{1, 0};
  sum += s_t{1e100, 1};
  BOOST_TEST_EQ(sum, (s_t{1e100, 2}));
  sum = s_t{0, 1};
  sum += s_t{1, 0};
  BOOST_TEST_EQ(sum, (s_t{1, 1}));

  accumulators::sum<double> a{3}, b{2}, c{3};
  BOOST_TEST_LT(b, c);
  BOOST_TEST_LE(b, c);
  BOOST_TEST_LE(a, c);
  BOOST_TEST_GT(a, b);
  BOOST_TEST_GE(a, b);
  BOOST_TEST_GE(a, c);

  BOOST_TEST_EQ(s_t{} += s_t{}, s_t{});

  return boost::report_errors();
}
