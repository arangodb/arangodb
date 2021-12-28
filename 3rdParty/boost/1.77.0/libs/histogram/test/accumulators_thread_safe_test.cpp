// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/ostream.hpp>
#include <boost/histogram/accumulators/thread_safe.hpp>
#include <sstream>
#include "throw_exception.hpp"
#include "utility_str.hpp"

using namespace boost::histogram;
using namespace std::literals;

#include <boost/histogram/detail/ignore_deprecation_warning_begin.hpp>

int main() {
  using ts_t = accumulators::thread_safe<int>;

  ts_t i;
  ++i;
  i += 1000;

  BOOST_TEST_EQ(i, 1001);
  BOOST_TEST_EQ(str(i), "1001"s);

  ts_t j{5};
  i = j;
  BOOST_TEST_EQ(i, 5);

  BOOST_TEST_EQ(ts_t{} += ts_t{}, ts_t{});

  return boost::report_errors();
}

#include <boost/histogram/detail/ignore_deprecation_warning_end.hpp>
