// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <vector>

using namespace boost::histogram;

int main() {
  // first and second arguments switched
  (void)make_histogram_with(axis::regular<>(3, 0, 1), std::vector<int>{});
}
