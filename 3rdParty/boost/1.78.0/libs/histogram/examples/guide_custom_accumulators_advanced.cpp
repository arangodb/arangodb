// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_accumulators_advanced

#include <boost/format.hpp>
#include <boost/histogram.hpp>
#include <iostream>
#include <sstream>

int main() {
  using namespace boost::histogram;

  // Accumulator accepts two samples and an optional weight and computes the mean of each.
  struct multi_mean {
    accumulators::mean<> mx, my;

    // called when no weight is passed
    void operator()(double x, double y) {
      mx(x);
      my(y);
    }

    // called when a weight is passed
    void operator()(weight_type<double> w, double x, double y) {
      mx(w, x);
      my(w, y);
    }
  };
  // Note: The implementation can be made more efficient by sharing the sum of weights.

  // Create a 1D histogram that uses the custom accumulator.
  auto h = make_histogram_with(dense_storage<multi_mean>(), axis::integer<>(0, 2));
  h(0, sample(1, 2));            // samples go to first cell
  h(0, sample(3, 4));            // samples go to first cell
  h(1, sample(5, 6), weight(2)); // samples go to second cell
  h(1, sample(7, 8), weight(3)); // samples go to second cell

  std::ostringstream os;
  for (auto&& bin : indexed(h)) {
    os << boost::format("index %i mean-x %.1f mean-y %.1f\n") % bin.index() %
              bin->mx.value() % bin->my.value();
  }
  std::cout << os.str() << std::flush;
  assert(os.str() == "index 0 mean-x 2.0 mean-y 3.0\n"
                     "index 1 mean-x 6.2 mean-y 7.2\n");
}

//]
