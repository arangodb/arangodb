// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_2d_axis

#include <boost/histogram.hpp>
#include <cassert>

int main() {
  using namespace boost::histogram;

  // axis which returns 1 if the input falls inside the unit circle and zero otherwise
  struct circle_axis {
    // accepts a 2D point in form of a std::tuple
    axis::index_type index(const std::tuple<double, double>& point) const {
      const auto x = std::get<0>(point);
      const auto y = std::get<1>(point);
      return x * x + y * y <= 1.0;
    }

    axis::index_type size() const { return 2; }
  };

  auto h1 = make_histogram(circle_axis());

  // fill looks normal for a histogram which has only one Nd-axis
  h1(0, 0);   // in
  h1(0, -1);  // in
  h1(0, 1);   // in
  h1(-1, 0);  // in
  h1(1, 0);   // in
  h1(1, 1);   // out
  h1(-1, -1); // out

  // 2D histogram, but only 1D index
  assert(h1.at(0) == 2); // out
  assert(h1.at(1) == 5); // in

  // other axes can be combined with a Nd-axis
  auto h2 = make_histogram(circle_axis(), axis::category<std::string>({"red", "blue"}));

  // now we need to pass arguments for Nd-axis explicitly as std::tuple
  h2(std::make_tuple(0, 0), "red");
  h2(std::make_tuple(1, 1), "blue");

  // 3D histogram, but only 2D index
  assert(h2.at(0, 0) == 0); // out, red
  assert(h2.at(0, 1) == 1); // out, blue
  assert(h2.at(1, 0) == 1); // in, red
  assert(h2.at(1, 1) == 0); // in, blue
}

//]
