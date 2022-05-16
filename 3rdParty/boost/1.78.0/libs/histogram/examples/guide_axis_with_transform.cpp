// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_axis_with_transform

#include <boost/histogram/axis/regular.hpp>
#include <limits>

int main() {
  using namespace boost::histogram;

  // make a regular axis with a log transform over [10, 100), [100, 1000), [1000, 10000)
  axis::regular<double, axis::transform::log> r_log{3, 10., 10000.};
  // log transform:
  // - useful when values vary dramatically in magnitude, like brightness of stars
  // - edges are not exactly at 10, 100, 1000, because of finite floating point precision
  // - values >= 0 but smaller than the starting value of the axis are mapped to -1
  // - values < 0 are mapped to `size()`, because the result of std::log(value) is NaN
  assert(r_log.index(10.1) == 0);
  assert(r_log.index(100.1) == 1);
  assert(r_log.index(1000.1) == 2);
  assert(r_log.index(1) == -1);
  assert(r_log.index(0) == -1);
  assert(r_log.index(-1) == 3);

  // make a regular axis with a sqrt transform over [4, 9), [9, 16), [16, 25)
  axis::regular<double, axis::transform::sqrt> r_sqrt{3, 4., 25.};
  // sqrt transform:
  // - bin widths are more mildly increasing compared to log transform
  // - axis starting at value == 0 is ok, sqrt(0) == 0 unlike log transform
  // - values < 0 are mapped to `size()`, because the result of std::sqrt(value) is NaN
  assert(r_sqrt.index(0) == -1);
  assert(r_sqrt.index(4.1) == 0);
  assert(r_sqrt.index(9.1) == 1);
  assert(r_sqrt.index(16.1) == 2);
  assert(r_sqrt.index(25.1) == 3);
  assert(r_sqrt.index(-1) == 3);

  // make a regular axis with a power transform x^1/3 over [1, 8), [8, 27), [27, 64)
  using pow_trans = axis::transform::pow;
  axis::regular<double, pow_trans> r_pow(pow_trans{1. / 3.}, 3, 1., 64.);
  // pow transform:
  // - generalization of the sqrt transform
  // - starting the axis at value == 0 is ok for power p > 0, 0^p == 0 for p > 0
  // - values < 0 are mapped to `size()` if power p is not a positive integer
  assert(r_pow.index(0) == -1);
  assert(r_pow.index(1.1) == 0);
  assert(r_pow.index(8.1) == 1);
  assert(r_pow.index(27.1) == 2);
  assert(r_pow.index(64.1) == 3);
  assert(r_pow.index(-1) == 3);
}

//]
