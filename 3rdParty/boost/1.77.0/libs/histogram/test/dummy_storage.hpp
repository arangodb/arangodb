// Copyright 2020 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_TEST_DUMMY_STORAGE_HPP
#define BOOST_HISTOGRAM_TEST_DUMMY_STORAGE_HPP

#include <algorithm>
#include <array>
#include <boost/histogram/detail/detect.hpp>
#include <ostream>
#include <type_traits>

template <class ValueType, bool Scaleable>
struct dummy_storage : std::array<ValueType, 10> {
  using base_t = std::array<ValueType, 10>;

  static constexpr bool has_threading_support = false;
  static constexpr bool scaleable =
      Scaleable && boost::histogram::detail::has_operator_rmul<ValueType, double>::value;

  std::size_t size_ = 0;

  std::size_t size() const { return size_; }

  void reset(std::size_t n) {
    assert(n < this->max_size());
    size_ = n;
  }

  auto end() { return this->begin() + size(); }
  auto end() const { return this->begin() + size(); }

  bool operator==(const dummy_storage& o) const {
    return std::equal(this->begin(), end(), o.begin(), o.end());
  }

  template <class S = dummy_storage>
  std::enable_if_t<S::scaleable, dummy_storage&> operator*=(double) {
    // do nothing, so it works with unscalable value types for testing purposes
    return *this;
  }
};

struct unscaleable {
  int value = 0;
  void operator++() { ++value; }
  bool operator==(const int& o) const { return value == o; }
};

inline std::ostream& operator<<(std::ostream& os, const unscaleable& x) {
  os << x.value;
  return os;
}

#endif
