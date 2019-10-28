// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

//[ getting_started_listing_03

#include <boost/format.hpp>
#include <boost/histogram.hpp>
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
  using namespace boost::histogram;

  /*
    Create a profile. Profiles does not only count entries in each cell, but
    also compute the mean of a sample value in each cell.
  */
  auto p = make_profile(axis::regular<>(5, 0.0, 1.0));

  /*
    Fill profile with data, usually this happens in a loop. You pass the sample
    with the `sample` helper function. The sample can be the first or last
    argument.
  */
  p(0.1, sample(1));
  p(0.15, sample(3));
  p(0.2, sample(4));
  p(0.9, sample(5));

  /*
    Iterate over bins and print profile.
  */
  std::ostringstream os;
  for (auto&& x : indexed(p)) {
    os << boost::format("bin %i [%3.1f, %3.1f) count %i mean %g\n") 
          % x.index() % x.bin().lower() % x.bin().upper() 
          % x->count() % x->value();
  }

  std::cout << os.str() << std::flush;
  assert(os.str() == "bin 0 [0.0, 0.2) count 2 mean 2\n"
                     "bin 1 [0.2, 0.4) count 1 mean 4\n"
                     "bin 2 [0.4, 0.6) count 0 mean 0\n"
                     "bin 3 [0.6, 0.8) count 0 mean 0\n"
                     "bin 4 [0.8, 1.0) count 1 mean 5\n");
}

//]
