// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_custom_accumulators

#include <array>
#include <boost/format.hpp>
#include <boost/histogram.hpp>
#include <boost/histogram/accumulators/mean.hpp>
#include <iostream>

int main() {
  using namespace boost::histogram;
  const auto axis = axis::regular<>(3, 0.0, 1.0);

  // Create a 1D-profile, which computes the mean of samples in each bin. The
  // factory function `make_profile` is provided by the library as a shorthand.
  auto h1 = make_histogram_with(dense_storage<accumulators::mean<>>(), axis);

  // Argument of `sample` is passed to accumulator.
  h1(0.0, sample(2)); // sample 2 goes to first bin
  h1(0.1, sample(2)); // sample 2 goes to first bin
  h1(0.4, sample(3)); // sample 3 goes to second bin
  h1(0.5, sample(4)); // sample 4 goes to second bin

  std::ostringstream os1;
  for (auto&& x : indexed(h1)) {
    // Accumulators usually have methods to access their state. Use the arrow
    // operator to access them. Here, `count()` gives the number of samples,
    // `value()` the mean, and `variance()` the variance estimate of the mean.
    os1 << boost::format("%i count %i mean %.1f variance %.1f\n") % x.index() %
               x->count() % x->value() % x->variance();
  }
  std::cout << os1.str() << std::flush;
  assert(os1.str() == "0 count 2 mean 2.0 variance 0.0\n"
                      "1 count 2 mean 3.5 variance 0.5\n"
                      "2 count 0 mean 0.0 variance 0.0\n");

  // Let's make a custom accumulator, which tracks the maximum of the samples. It must
  // have a call operator that accepts the argument of the `sample` function. The return
  // value of the call operator is ignored.
  struct max {
    void operator()(double x) {
      if (x > value) value = x;
    }
    double value = 0; // value is initialized to zero
  };

  // Create a histogram that uses the custom accumulator.
  auto h2 = make_histogram_with(dense_storage<max>(), axis);
  h2(0.0, sample(2));   // sample 2 goes to first bin
  h2(0.1, sample(2.5)); // sample 2.5 goes to first bin
  h2(0.4, sample(3));   // sample 3 goes to second bin
  h2(0.5, sample(4));   // sample 4 goes to second bin

  std::ostringstream os2;
  for (auto&& x : indexed(h2)) {
    os2 << boost::format("%i value %.1f\n") % x.index() % x->value;
  }
  std::cout << os2.str() << std::flush;
  assert(os2.str() == "0 value 2.5\n"
                      "1 value 4.0\n"
                      "2 value 0.0\n");
}

//]
