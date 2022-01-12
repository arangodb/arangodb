// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_fill_weighted_profile

#include <boost/format.hpp>
#include <boost/histogram.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
  using namespace boost::histogram;
  using namespace boost::histogram::literals; // _c suffix creates compile-time numbers

  // make 2D weighted profile
  auto h = make_weighted_profile(axis::integer<>(0, 2), axis::integer<>(0, 2));

  // The mean is computed from the values marked with the sample() helper function.
  // Weights can be passed as well. The `sample` and `weight` arguments can appear in any
  // order, but they must be the first or last arguments.
  h(0, 0, sample(1));            // sample goes to cell (0, 0); weight is 1
  h(0, 0, sample(2), weight(3)); // sample goes to cell (0, 0); weight is 3
  h(1, 0, sample(3));            // sample goes to cell (1, 0); weight is 1
  h(1, 0, sample(4));            // sample goes to cell (1, 0); weight is 1
  h(0, 1, sample(5));            // sample goes to cell (1, 0); weight is 1
  h(0, 1, sample(6));            // sample goes to cell (1, 0); weight is 1
  h(1, 1, weight(4), sample(7)); // sample goes to cell (1, 1); weight is 4
  h(weight(5), sample(8), 1, 1); // sample goes to cell (1, 1); weight is 5

  std::ostringstream os;
  for (auto&& x : indexed(h)) {
    const auto i = x.index(0_c);
    const auto j = x.index(1_c);
    const auto m = x->value();    // weighted mean
    const auto v = x->variance(); // estimated variance of weighted mean
    os << boost::format("index %i,%i mean %.1f variance %.1f\n") % i % j % m % v;
  }

  std::cout << os.str() << std::flush;

  assert(os.str() == "index 0,0 mean 1.8 variance 0.5\n"
                     "index 1,0 mean 3.5 variance 0.5\n"
                     "index 0,1 mean 5.5 variance 0.5\n"
                     "index 1,1 mean 7.6 variance 0.5\n");
}

//]
