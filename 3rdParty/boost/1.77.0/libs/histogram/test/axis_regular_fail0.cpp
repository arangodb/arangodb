// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/histogram/axis/regular.hpp>

using namespace boost::histogram;

int main() {
  // regular axis requires a floating point value type
  (void)axis::regular<int>(1, 2, 3);
}
