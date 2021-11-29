// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/histogram/axis/regular.hpp>

using namespace boost::histogram;

int main() {
  // circular regular axis cannot be growing
  (void)axis::regular<double, boost::use_default, boost::use_default,
                      decltype(axis::option::circular | axis::option::growth)>(1, 2, 3);
}
