// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_fill_histogram

#include <boost/histogram.hpp>
#include <cassert>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>

int main() {
  using namespace boost::histogram;

  auto h = make_histogram(axis::integer<>(0, 3), axis::regular<>(2, 0.0, 1.0));

  // fill histogram, number of arguments must be equal to number of axes,
  // types must be convertible to axis value type (here integer and double)
  h(0, 0.2); // increase a cell value by one
  h(2, 0.5); // increase another cell value by one

  // fills from a tuple are also supported; passing a tuple of wrong size
  // causes an error at compile-time or an assertion at runtime in debug mode
  auto xy = std::make_tuple(1, 0.3);
  h(xy);

  // chunk-wise filling is also supported and more efficient, make some data...
  std::vector<double> xy2[2] = {{0, 2, 5}, {0.8, 0.4, 0.7}};

  // ... and call fill method
  h.fill(xy2);

  // once histogram is filled, access individual cells using operator[] or at(...)
  // - operator[] can only accept a single argument in the current version of C++,
  //   it is convenient when you have a 1D histogram
  // - at(...) can accept several values, so use this by default
  // - underflow bins are at index -1, overflow bins at index `size()`
  // - passing an invalid index triggers a std::out_of_range exception
  assert(h.at(0, 0) == 1);
  assert(h.at(0, 1) == 1);
  assert(h.at(1, 0) == 1);
  assert(h.at(1, 1) == 0);
  assert(h.at(2, 0) == 1);
  assert(h.at(2, 1) == 1);
  assert(h.at(-1, -1) == 0); // underflow for axis 0 and 1
  assert(h.at(-1, 0) == 0);  // underflow for axis 0, normal bin for axis 1
  assert(h.at(-1, 2) == 0);  // underflow for axis 0, overflow for axis 1
  assert(h.at(3, 1) == 1);   // overflow for axis 0, normal bin for axis 1

  // iteration over values works, but see next example for a better way
  // - iteration using begin() and end() includes under- and overflow bins
  // - iteration order is an implementation detail and should not be relied upon
  const double sum = std::accumulate(h.begin(), h.end(), 0.0);
  assert(sum == 6);
}

//]
