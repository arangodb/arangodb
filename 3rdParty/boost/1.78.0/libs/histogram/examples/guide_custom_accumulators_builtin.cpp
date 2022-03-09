// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_accumulators_builtin

#include <boost/format.hpp>
#include <boost/histogram.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
  using namespace boost::histogram;
  using mean = accumulators::mean<>;

  // Create a 1D-profile, which computes the mean of samples in each bin.
  auto h = make_histogram_with(dense_storage<mean>(), axis::integer<>(0, 2));
  // The factory function `make_profile` is provided as a shorthand for this, so this is
  // equivalent to the previous line: auto h = make_profile(axis::integer<>(0, 2));

  // An argument marked as `sample` is passed to the accumulator.
  h(0, sample(1)); // sample goes to first cell
  h(0, sample(2)); // sample goes to first cell
  h(1, sample(3)); // sample goes to second cell
  h(1, sample(4)); // sample goes to second cell

  std::ostringstream os;
  for (auto&& x : indexed(h)) {
    // Accumulators usually have methods to access their state. Use the arrow
    // operator to access them. Here, `count()` gives the number of samples,
    // `value()` the mean, and `variance()` the variance estimate of the mean.
    os << boost::format("index %i count %i mean %.1f variance %.1f\n") % x.index() %
              x->count() % x->value() % x->variance();
  }
  std::cout << os.str() << std::flush;
  assert(os.str() == "index 0 count 2 mean 1.5 variance 0.5\n"
                     "index 1 count 2 mean 3.5 variance 0.5\n");
}

//]
