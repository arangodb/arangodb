// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_histogram_operators

#include <boost/histogram.hpp>
#include <cassert>
#include <vector>

int main() {
  using namespace boost::histogram;

  // make two histograms
  auto h1 = make_histogram(axis::regular<>(2, -1.0, 1.0));
  auto h2 = make_histogram(axis::regular<>(2, -1.0, 1.0));

  h1(-0.5); // counts are: 1 0
  h2(0.5);  // counts are: 0 1

  // add them
  auto h3 = h1;
  h3 += h2; // counts are: 1 1

  // adding multiple histograms at once is likely to be optimized by the compiler so that
  // superfluous temporaries avoided, but no guarantees are given; use this equivalent
  // code when you want to make sure: h4 = h1; h4 += h2; h4 += h3;
  auto h4 = h1 + h2 + h3; // counts are: 2 2

  assert(h4.at(0) == 2 && h4.at(1) == 2);

  // multiply by number, h4 *= 2 also works
  auto h5 = h4 * 2; // counts are: 4 4

  // divide by number; s4 /= 4 also works
  auto h6 = h5 / 4; // counts are: 1 1

  assert(h6.at(0) == 1 && h6.at(1) == 1);
  assert(h6 != h5 && h5 == 4 * h6);

  // note the special effect of multiplication on weight_storage
  auto h = make_histogram_with(weight_storage(), axis::regular<>(2, -1.0, 1.0));
  h(-0.5);

  // counts are: 1 0
  assert(h.at(0).value() == 1 && h.at(1).value() == 0);

  auto h_sum = h + h;
  auto h_mul = 2 * h;

  // values are the same as expected...
  assert(h_sum.at(0).value() == h_mul.at(0).value());
  // ... but variances differ
  assert(h_sum.at(0).variance() == 2 && h_mul.at(0).variance() == 4);

  // equality operator checks variances, so histograms are not equal
  assert(h_sum != h_mul);
}

//]
