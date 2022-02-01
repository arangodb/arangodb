// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_accumulators_simple

#include <boost/format.hpp>
#include <boost/histogram.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
  using namespace boost::histogram;

  // A custom accumulator which tracks the maximum of the samples.
  // It must have a call operator that accepts the argument of the `sample` function.
  struct maximum {
    // return value is ignored, so we use void
    void operator()(double x) {
      if (x > value) value = x;
    }
    double value = 0; // value is public and initialized to zero
  };

  // Create 1D histogram that uses the custom accumulator.
  auto h = make_histogram_with(dense_storage<maximum>(), axis::integer<>(0, 2));
  h(0, sample(1.0)); // sample goes to first cell
  h(0, sample(2.0)); // sample goes to first cell
  h(1, sample(3.0)); // sample goes to second cell
  h(1, sample(4.0)); // sample goes to second cell

  std::ostringstream os;
  for (auto&& x : indexed(h)) {
    os << boost::format("index %i maximum %.1f\n") % x.index() % x->value;
  }
  std::cout << os.str() << std::flush;
  assert(os.str() == "index 0 maximum 2.0\n"
                     "index 1 maximum 4.0\n");
}

//]
