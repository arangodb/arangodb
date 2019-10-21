// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_axis_circular

#include <boost/histogram/axis.hpp>
#include <limits>

int main() {
  using namespace boost::histogram;

  // make a circular regular axis ... [0, 180), [180, 360), [0, 180) ....
  using opts = decltype(axis::option::overflow | axis::option::circular);
  auto r = axis::regular<double, use_default, use_default, opts>{2, 0., 360.};
  assert(r.index(-180) == 1);
  assert(r.index(0) == 0);
  assert(r.index(180) == 1);
  assert(r.index(360) == 0);
  assert(r.index(540) == 1);
  assert(r.index(720) == 0);
  // special values are mapped to the overflow bin index
  assert(r.index(std::numeric_limits<double>::infinity()) == 2);
  assert(r.index(-std::numeric_limits<double>::infinity()) == 2);
  assert(r.index(std::numeric_limits<double>::quiet_NaN()) == 2);

  // since the regular axis is the most common circular axis, there exists an alias
  auto c = axis::circular<>{2, 0., 360.};
  assert(r == c);

  // make a circular integer axis
  auto i = axis::integer<int, use_default, axis::option::circular_t>{1, 4};
  assert(i.index(0) == 2);
  assert(i.index(1) == 0);
  assert(i.index(2) == 1);
  assert(i.index(3) == 2);
  assert(i.index(4) == 0);
  assert(i.index(5) == 1);
}

//]
