// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/detail/limits.hpp>
#include <limits>

using namespace boost::histogram::detail;

int main() {

  BOOST_TEST_EQ(lowest<int>(), std::numeric_limits<int>::min());
  BOOST_TEST_EQ(lowest<float>(), -std::numeric_limits<float>::infinity());
  BOOST_TEST_EQ(lowest<double>(), -std::numeric_limits<double>::infinity());

  BOOST_TEST_EQ(highest<int>(), std::numeric_limits<int>::max());
  BOOST_TEST_EQ(highest<float>(), std::numeric_limits<float>::infinity());
  BOOST_TEST_EQ(highest<double>(), std::numeric_limits<double>::infinity());

  return boost::report_errors();
}
