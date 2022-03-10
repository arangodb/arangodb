// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_fill_profile

#include <boost/format.hpp>
#include <boost/histogram.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <tuple>

int main() {
  using namespace boost::histogram;

  // make a profile, it computes the mean of the samples in each histogram cell
  auto h = make_profile(axis::integer<>(0, 3));

  // mean is computed from the values marked with the sample() helper function
  h(0, sample(1)); // sample goes to cell 0
  h(0, sample(2)); // sample goes to cell 0
  h(1, sample(3)); // sample goes to cell 1
  h(sample(4), 1); // sample goes to cell 1; sample can be first or last argument

  // fills from tuples are also supported, 5 and 6 go to cell 2
  auto a = std::make_tuple(2, sample(5));
  auto b = std::make_tuple(sample(6), 2);
  h(a);
  h(b);

  // builtin accumulators have methods to access their state
  std::ostringstream os;
  for (auto&& x : indexed(h)) {
    // use `.` to access methods of accessor, like `index()`
    // use `->` to access methods of accumulator
    const auto i = x.index();
    const auto n = x->count();     // how many samples are in this bin
    const auto vl = x->value();    // mean value
    const auto vr = x->variance(); // estimated variance of the mean value
    os << boost::format("index %i count %i value %.1f variance %.1f\n") % i % n % vl % vr;
  }

  std::cout << os.str() << std::flush;

  assert(os.str() == "index 0 count 2 value 1.5 variance 0.5\n"
                     "index 1 count 2 value 3.5 variance 0.5\n"
                     "index 2 count 2 value 5.5 variance 0.5\n");
}

//]
