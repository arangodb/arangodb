// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_histogram_streaming

#include <boost/histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
  using namespace boost::histogram;

  std::ostringstream os;

  // width of histogram can be set like this; if it is not set, the library attempts to
  // determine the terminal width and choses the histogram width accordingly
  os.width(78);

  auto h1 = make_histogram(axis::regular<>(5, -1.0, 1.0, "axis 1"));
  h1.at(0) = 2;
  h1.at(1) = 4;
  h1.at(2) = 3;
  h1.at(4) = 1;

  // 1D histograms are rendered as an ASCII drawing
  os << h1;

  auto h2 = make_histogram(axis::regular<>(2, -1.0, 1.0, "axis 1"),
                           axis::category<std::string>({"red", "blue"}, "axis 2"));

  // higher dimensional histograms just have their cell counts listed
  os << h2;

  std::cout << os.str() << std::endl;

  assert(
      os.str() ==
      "histogram(regular(5, -1, 1, metadata=\"axis 1\", options=underflow | overflow))\n"
      "               ┌─────────────────────────────────────────────────────────────┐\n"
      "[-inf,   -1) 0 │                                                             │\n"
      "[  -1, -0.6) 2 │██████████████████████████████                               │\n"
      "[-0.6, -0.2) 4 │████████████████████████████████████████████████████████████ │\n"
      "[-0.2,  0.2) 3 │█████████████████████████████████████████████                │\n"
      "[ 0.2,  0.6) 0 │                                                             │\n"
      "[ 0.6,    1) 1 │███████████████                                              │\n"
      "[   1,  inf) 0 │                                                             │\n"
      "               └─────────────────────────────────────────────────────────────┘\n"
      "histogram(\n"
      "  regular(2, -1, 1, metadata=\"axis 1\", options=underflow | overflow)\n"
      "  category(\"red\", \"blue\", metadata=\"axis 2\", options=overflow)\n"
      "  (-1 0): 0 ( 0 0): 0 ( 1 0): 0 ( 2 0): 0 (-1 1): 0 ( 0 1): 0\n"
      "  ( 1 1): 0 ( 2 1): 0 (-1 2): 0 ( 0 2): 0 ( 1 2): 0 ( 2 2): 0\n"
      ")");
}

//]
