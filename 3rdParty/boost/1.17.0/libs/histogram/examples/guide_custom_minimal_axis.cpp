// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_minimal_axis

#include <boost/histogram.hpp>
#include <cassert>

int main() {
  using namespace boost::histogram;

  // stateless axis which returns 1 if the input is even and 0 otherwise
  struct even_odd_axis {
    axis::index_type index(int x) const { return x % 2; }
    axis::index_type size() const { return 2; }
  };

  // threshold axis which returns 1 if the input is above threshold
  struct threshold_axis {
    threshold_axis(double x) : thr(x) {}
    axis::index_type index(double x) const { return x >= thr; }
    axis::index_type size() const { return 2; }
    double thr;
  };

  auto h = make_histogram(even_odd_axis(), threshold_axis(3.0));

  h(0, 2.0);
  h(1, 4.0);
  h(2, 4.0);

  assert(h.at(0, 0) == 1); // even, below threshold
  assert(h.at(0, 1) == 1); // even, above threshold
  assert(h.at(1, 0) == 0); // odd, below threshold
  assert(h.at(1, 1) == 1); // odd, above threshold
}

//]
