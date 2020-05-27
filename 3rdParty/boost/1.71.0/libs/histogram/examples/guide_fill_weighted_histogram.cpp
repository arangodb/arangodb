// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_fill_weighted_histogram

#include <boost/histogram.hpp>

int main() {
  using namespace boost::histogram;

  // Create a histogram with weight counters that keep track of a variance estimate.
  auto h = make_weighted_histogram(axis::regular<>(3, 0.0, 1.0));

  h(0.0, weight(1)); // weight 1 goes to first bin
  h(0.1, weight(2)); // weight 2 goes to first bin
  h(0.4, weight(3)); // weight 3 goes to second bin
  h(0.5, weight(4)); // weight 4 goes to second bin

  // Weight counters have methods to access the value (sum of weights) and the variance
  // (sum of weights squared, why this gives the variance is explained in the rationale)
  assert(h[0].value() == 1 + 2);
  assert(h[0].variance() == 1 * 1 + 2 * 2);
  assert(h[1].value() == 3 + 4);
  assert(h[1].variance() == 3 * 3 + 4 * 4);
}

//]
