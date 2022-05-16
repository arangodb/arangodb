// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_axis_with_uoflow_off

#include <boost/histogram.hpp>
#include <string>

int main() {
  using namespace boost::histogram;

  // create a 1d-histogram over integer values from 1 to 6
  auto h1 = make_histogram(axis::integer<int>(1, 7));
  // axis has size 6...
  assert(h1.axis().size() == 6);
  // ... but histogram has size 8, because of overflow and underflow bins
  assert(h1.size() == 8);

  // create a 1d-histogram for throws of a six-sided die without extra bins,
  // since the values cannot be smaller than 1 or larger than 6
  auto h2 = make_histogram(axis::integer<int, use_default, axis::option::none_t>(1, 7));
  // now size of axis and histogram is equal
  assert(h2.axis().size() == 6);
  assert(h2.size() == 6);
}

//]
