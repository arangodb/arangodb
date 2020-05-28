// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

//[ getting_started_listing_04

#include <algorithm>           // std::max_element
#include <boost/format.hpp>    // only needed for printing
#include <boost/histogram.hpp> // make_histogram, integer, indexed
#include <iostream>            // std::cout, std::endl
#include <sstream>             // std::ostringstream

int main() {
  using namespace boost::histogram;
  using namespace boost::histogram::literals;

  /*
    We make a 3d histogram for color values (r, g, b) in an image. We assume the values
    are of type char. The value range then is [0, 256).
  */
  auto h = make_histogram(
    axis::integer<>(0, 256, "r"),
    axis::integer<>(0, 256, "g"),
    axis::integer<>(0, 256, "b")
  );

  /*
    We don't have real image data, so fill some fake data.
  */
  h(1, 2, 3);
  h(1, 2, 3);
  h(0, 1, 0);

  /*
    Now let's say we want to know which color is most common. We can use std::max_element on an
    indexed range for that.
  */
  auto ind = indexed(h);
  auto max_it = std::max_element(ind.begin(), ind.end());

  /*
    max_it is the iterator to the maximum counter. This is the special iterator from indexed_range,
    you can use it to access the cell index as well and the bin values. You need to
    double-dereference it to get to the cell count.
  */
  std::ostringstream os;
  os << boost::format("count=%i rgb=(%i, %i, %i)")
        % **max_it % max_it->bin(0_c) % max_it->bin(1) % max_it->bin(2);

  std::cout << os.str() << std::endl;

  assert(os.str() == "count=2 rgb=(1, 2, 3)");
}

//]
