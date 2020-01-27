// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ACCUMULATORS_SUM_HPP
#define BOOST_HISTOGRAM_ACCUMULATORS_SUM_HPP

#include <boost/histogram/fwd.hpp>
#include <cmath>
#include <type_traits>

namespace boost {
namespace histogram {
namespace accumulators {

/**
  Uses Neumaier algorithm to compute accurate sums.

  The algorithm uses memory for two floats and is three to
  five times slower compared to a simple floating point
  number used to accumulate a sum, but the relative error
  of the sum is at the level of the machine precision,
  independent of the number of samples.

  A. Neumaier, Zeitschrift fuer Angewandte Mathematik
  und Mechanik 54 (1974) 39-51.
*/
template <typename RealType>
class sum {
public:
  sum() = default;

  /// Initialize sum to value
  explicit sum(const RealType& value) noexcept : large_(value) {}

  /// Set sum to value
  sum& operator=(const RealType& value) noexcept {
    large_ = value;
    small_ = 0;
    return *this;
  }

  /// Increment sum by one
  sum& operator++() { return operator+=(1); }

  /// Increment sum by value
  sum& operator+=(const RealType& value) {
    auto temp = large_ + value; // prevent optimization
    if (std::abs(large_) >= std::abs(value))
      small_ += (large_ - temp) + value;
    else
      small_ += (value - temp) + large_;
    large_ = temp;
    return *this;
  }

  /// Scale by value
  sum& operator*=(const RealType& value) {
    large_ *= value;
    small_ *= value;
    return *this;
  }

  template <class T>
  bool operator==(const sum<T>& rhs) const noexcept {
    return large_ == rhs.large_ && small_ == rhs.small_;
  }

  template <class T>
  bool operator!=(const T& rhs) const noexcept {
    return !operator==(rhs);
  }

  /// Return large part of the sum.
  const RealType& large() const { return large_; }

  /// Return small part of the sum.
  const RealType& small() const { return small_; }

  // allow implicit conversion to RealType
  operator RealType() const { return large_ + small_; }

  template <class Archive>
  void serialize(Archive&, unsigned /* version */);

private:
  RealType large_ = RealType();
  RealType small_ = RealType();
};

} // namespace accumulators
} // namespace histogram
} // namespace boost

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED
namespace std {
template <class T, class U>
struct common_type<boost::histogram::accumulators::sum<T>,
                   boost::histogram::accumulators::sum<U>> {
  using type = boost::histogram::accumulators::sum<common_type_t<T, U>>;
};
} // namespace std
#endif

#endif
