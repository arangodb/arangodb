// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_TEST_UTILITY_AXIS_HPP
#define BOOST_HISTOGRAM_TEST_UTILITY_AXIS_HPP

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/fwd.hpp>

namespace boost {
namespace histogram {

template <class Axis>
void test_axis_iterator(const Axis& a, axis::index_type begin, axis::index_type end) {
  for (auto bin : a) {
    BOOST_TEST_EQ(bin, a.bin(begin));
    ++begin;
  }
  BOOST_TEST_EQ(begin, end);
  auto rit = a.rbegin();
  for (; rit != a.rend(); ++rit) {
    --begin;
    BOOST_TEST_EQ(*rit, a.bin(begin));
  }
}

namespace axis {
bool operator==(const null_type&, const null_type&) { return true; }
} // namespace axis

} // namespace histogram
} // namespace boost

#endif
