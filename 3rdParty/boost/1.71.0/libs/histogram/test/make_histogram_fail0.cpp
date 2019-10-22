// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/histogram/make_histogram.hpp>

using namespace boost::histogram;

int main() {
  // argument is not an axis
  (void)make_histogram(std::vector<int>{});
}
