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

  // make a 2d histogram
  auto h = make_histogram(axis::regular<>(4, 0.0, 4.0), axis::regular<>(4, -1.0, 1.0));

  h(0, -0.9);
  h(1, 0.9);
  h(2, 0.1);
  h(3, 0.1);

  // shrink the first axis to remove the highest bin
  // rebin the second axis by merging pairs of adjacent bins
  auto h2 = algorithm::reduce(h, algorithm::shrink(0, 0.0, 3.0), algorithm::rebin(1, 2));

  // reduce does not remove counts if the histogram has underflow/overflow bins
  assert(algorithm::sum(h) == 4 && algorithm::sum(h2) == 4);

  assert(h2.axis(0).size() == 3);
  assert(h2.axis(0).bin(0).lower() == 0.0);
  assert(h2.axis(0).bin(2).upper() == 3.0);
  assert(h2.axis(1).size() == 2);
  assert(h2.axis(1).bin(0).lower() == -1.0);
  assert(h2.axis(1).bin(1).upper() == 1.0);
}

//]
