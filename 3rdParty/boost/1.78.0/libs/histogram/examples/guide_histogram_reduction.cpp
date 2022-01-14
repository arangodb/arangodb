// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_histogram_reduction

#include <boost/histogram.hpp>
#include <cassert>

int main() {
  using namespace boost::histogram;
  // import reduce commands into local namespace to save typing
  using algorithm::rebin;
  using algorithm::shrink;
  using algorithm::slice;

  // make a 2d histogram
  auto h = make_histogram(axis::regular<>(4, 0.0, 4.0), axis::regular<>(4, -2.0, 2.0));

  h(0, -0.9);
  h(1, 0.9);
  h(2, 0.1);
  h(3, 0.1);

  // reduce takes positional commands which are applied to the axes in order
  // - shrink is applied to the first axis; the new axis range is 0.0 to 3.0
  // - rebin is applied to the second axis; pairs of adjacent bins are merged
  auto h2 = algorithm::reduce(h, shrink(0.0, 3.0), rebin(2));

  assert(h2.axis(0) == axis::regular<>(3, 0.0, 3.0));
  assert(h2.axis(1) == axis::regular<>(2, -2.0, 2.0));

  // reduce does not change the total count if the histogram has underflow/overflow bins
  assert(algorithm::sum(h) == 4 && algorithm::sum(h2) == 4);

  // One can also explicitly specify the index of the axis in the histogram on which the
  // command should act, by using this index as the the first parameter. The position of
  // the command in the argument list of reduce is then ignored. We use this to slice only
  // the second axis (axis has index 1 in the histogram) from bin index 2 to 4.
  auto h3 = algorithm::reduce(h, slice(1, 2, 4));

  assert(h3.axis(0) == h.axis(0)); // unchanged
  assert(h3.axis(1) == axis::regular<>(2, 0.0, 2.0));
}

//]
