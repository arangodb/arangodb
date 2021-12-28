// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_TEST_STD_OSTREAM_HPP
#define BOOST_HISTOGRAM_TEST_STD_OSTREAM_HPP

#include <boost/mp11/tuple.hpp>
#include <ostream>
#include <utility>
#include <vector>

namespace std {
// never add to std, we only do it here to get ADL working :(
template <class T>
ostream& operator<<(ostream& os, const vector<T>& v) {
  os << "[ ";
  for (const auto& x : v) os << x << " ";
  os << "]";
  return os;
}

template <class... Ts>
ostream& operator<<(ostream& os, const std::tuple<Ts...>& t) {
  os << "[ ";
  ::boost::mp11::tuple_for_each(t, [&os](const auto& x) { os << x << " "; });
  os << "]";
  return os;
}

template <class T, class U>
ostream& operator<<(ostream& os, const std::pair<T, U>& t) {
  os << "[ " << t.first << " " << t.second << " ]";
  return os;
}
} // namespace std

#endif
