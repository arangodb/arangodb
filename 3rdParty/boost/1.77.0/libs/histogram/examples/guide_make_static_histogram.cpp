// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ guide_make_static_histogram

#include <boost/histogram.hpp>
#include <cassert>

int main() {
  using namespace boost::histogram;

  // create a 1d-histogram in default configuration which
  // covers the real line from -1 to 1 in 100 bins
  auto h = make_histogram(axis::regular<>(100, -1.0, 1.0));

  // rank is the number of axes
  assert(h.rank() == 1);
}

//]
