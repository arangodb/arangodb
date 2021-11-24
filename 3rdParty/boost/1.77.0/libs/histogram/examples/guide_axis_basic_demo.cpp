// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_axis_basic_demo

#include <boost/histogram/axis.hpp>
#include <limits>

int main() {
  using namespace boost::histogram;

  // make a regular axis with 10 bins over interval from 1.5 to 2.5
  auto r = axis::regular<>{10, 1.5, 2.5};
  // `<>` is needed in C++14 because the axis is templated,
  // in C++17 you can do: auto r = axis::regular{10, 1.5, 2.5};
  assert(r.size() == 10);
  // alternatively, you can define the step size with the `step` marker
  auto r_step = axis::regular<>{axis::step(0.1), 1.5, 2.5};
  assert(r_step == r);

  // histogram uses the `index` method to convert values to indices
  // note: intervals of builtin axis types are always semi-open [a, b)
  assert(r.index(1.5) == 0);
  assert(r.index(1.6) == 1);
  assert(r.index(2.4) == 9);
  // index for a value below the start of the axis is always -1
  assert(r.index(1.0) == -1);
  assert(r.index(-std::numeric_limits<double>::infinity()) == -1);
  // index for a value below the above the end of the axis is always `size()`
  assert(r.index(3.0) == 10);
  assert(r.index(std::numeric_limits<double>::infinity()) == 10);
  // index for not-a-number is also `size()` by convention
  assert(r.index(std::numeric_limits<double>::quiet_NaN()) == 10);

  // make a variable axis with 3 bins [-1.5, 0.1), [0.1, 0.3), [0.3, 10)
  auto v = axis::variable<>{-1.5, 0.1, 0.3, 10.};
  assert(v.index(-2.0) == -1);
  assert(v.index(-1.5) == 0);
  assert(v.index(0.1) == 1);
  assert(v.index(0.3) == 2);
  assert(v.index(10) == 3);
  assert(v.index(20) == 3);

  // make an integer axis with 3 bins at -1, 0, 1
  auto i = axis::integer<>{-1, 2};
  assert(i.index(-2) == -1);
  assert(i.index(-1) == 0);
  assert(i.index(0) == 1);
  assert(i.index(1) == 2);
  assert(i.index(2) == 3);

  // make an integer axis called "foo"
  auto i_with_label = axis::integer<>{-1, 2, "foo"};
  // all builtin axis types allow you to pass some optional metadata as the last
  // argument in the constructor; a string by default, but can be any copyable type

  // two axis do not compare equal if they differ in their metadata
  assert(i != i_with_label);

  // integer axis also work well with unscoped enums
  enum { red, blue };
  auto i_for_enum = axis::integer<>{red, blue + 1};
  assert(i_for_enum.index(red) == 0);
  assert(i_for_enum.index(blue) == 1);

  // make a category axis from a scoped enum and/or if the identifiers are not consecutive
  enum class Bar { red = 12, blue = 6 };
  auto c = axis::category<Bar>{Bar::red, Bar::blue};
  assert(c.index(Bar::red) == 0);
  assert(c.index(Bar::blue) == 1);
  // c.index(12) is a compile-time error, since the argument must be of type `Bar`

  // category axis can be created for any copyable and equal-comparable type
  auto c_str = axis::category<std::string>{"red", "blue"};
  assert(c_str.index("red") == 0);
  assert(c_str.index("blue") == 1);
}

//]
