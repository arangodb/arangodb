// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

//[ getting_started_listing_05

//////////////// Begin: put this in header file ////////////////////

#include <algorithm>           // std::max_element
#include <boost/format.hpp>    // only needed for printing
#include <boost/histogram.hpp> // make_histogram, integer, indexed
#include <iostream>            // std::cout, std::endl
#include <sstream>             // std::ostringstream
#include <tuple>
#include <vector>

// use this when axis configuration is fix to get highest performance
struct HolderOfStaticHistogram {
  // put axis types here
  using axes_t = std::tuple<
    boost::histogram::axis::regular<>,
    boost::histogram::axis::integer<>    
  >;
  using hist_t = boost::histogram::histogram<axes_t>;
  hist_t hist_;
};

// use this when axis configuration should be flexible
struct HolderOfDynamicHistogram {
  // put all axis types here that you are going to use
  using axis_t = boost::histogram::axis::variant<
    boost::histogram::axis::regular<>,
    boost::histogram::axis::variable<>,    
    boost::histogram::axis::integer<>    
  >;
  using axes_t = std::vector<axis_t>;
  using hist_t = boost::histogram::histogram<axes_t>;
  hist_t hist_;
};

//////////////// End: put this in header file ////////////////////

int main() {
  using namespace boost::histogram;

  HolderOfStaticHistogram hs;
  hs.hist_ = make_histogram(axis::regular<>(5, 0, 1), axis::integer<>(0, 3));
  // now assign a different histogram
  hs.hist_ = make_histogram(axis::regular<>(3, 1, 2), axis::integer<>(4, 6));
  // hs.hist_ = make_histogram(axis::regular<>(5, 0, 1)); does not work;
  // the static histogram cannot change the number or order of axis types

  HolderOfDynamicHistogram hd;
  hd.hist_ = make_histogram(axis::regular<>(5, 0, 1), axis::integer<>(0, 3));
  // now assign a different histogram
  hd.hist_ = make_histogram(axis::regular<>(3, -1, 2));
  // and assign another
  hd.hist_ = make_histogram(axis::integer<>(0, 5), axis::integer<>(3, 5));
}

//]
