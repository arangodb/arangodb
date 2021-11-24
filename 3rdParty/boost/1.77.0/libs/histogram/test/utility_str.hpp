// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_TEST_UTILITY_STR_HPP
#define BOOST_HISTOGRAM_TEST_UTILITY_STR_HPP

#include <sstream>

template <class T>
auto str(const T& t, int w = 0, bool left = true) {
  std::ostringstream os;
  auto saved = os.width();
  os.width(w);
  if (left)
    os << std::left;
  else
    os << std::right;
  os << t;
  os.width(saved);
  return os.str();
}

#endif
