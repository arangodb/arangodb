// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ACCUMULATORS_WEIGHTED_SUM_HPP
#define BOOST_HISTOGRAM_ACCUMULATORS_WEIGHTED_SUM_HPP

#include <boost/histogram/fwd.hpp>
#include <type_traits>

namespace boost {
namespace histogram {
namespace accumulators {

/// Holds sum of weights and its variance estimate
template <typename RealType>
class weighted_sum {
public:
  weighted_sum() = default;
  explicit weighted_sum(const RealType& value) noexcept
      : sum_of_weights_(value), sum_of_weights_squared_(value) {}
  weighted_sum(const RealType& value, const RealType& variance) noexcept
      : sum_of_weights_(value), sum_of_weights_squared_(variance) {}

  /// Increment by one.
  weighted_sum& operator++() { return operator+=(1); }

  /// Increment by value.
  template <typename T>
  weighted_sum& operator+=(const T& value) {
    sum_of_weights_ += value;
    sum_of_weights_squared_ += value * value;
    return *this;
  }

  /// Added another weighted sum.
  template <typename T>
  weighted_sum& operator+=(const weighted_sum<T>& rhs) {
    sum_of_weights_ += static_cast<RealType>(rhs.sum_of_weights_);
    sum_of_weights_squared_ += static_cast<RealType>(rhs.sum_of_weights_squared_);
    return *this;
  }

  /// Scale by value.
  weighted_sum& operator*=(const RealType& x) {
    sum_of_weights_ *= x;
    sum_of_weights_squared_ *= x * x;
    return *this;
  }

  bool operator==(const RealType& rhs) const noexcept {
    return sum_of_weights_ == rhs && sum_of_weights_squared_ == rhs;
  }

  template <typename T>
  bool operator==(const weighted_sum<T>& rhs) const noexcept {
    return sum_of_weights_ == rhs.sum_of_weights_ &&
           sum_of_weights_squared_ == rhs.sum_of_weights_squared_;
  }

  template <typename T>
  bool operator!=(const T& rhs) const noexcept {
    return !operator==(rhs);
  }

  /// Return value of the sum.
  const RealType& value() const noexcept { return sum_of_weights_; }

  /// Return estimated variance of the sum.
  const RealType& variance() const noexcept { return sum_of_weights_squared_; }

  // lossy conversion must be explicit
  template <class T>
  explicit operator T() const {
    return static_cast<T>(sum_of_weights_);
  }

  template <class Archive>
  void serialize(Archive&, unsigned /* version */);

private:
  RealType sum_of_weights_ = RealType();
  RealType sum_of_weights_squared_ = RealType();
};

} // namespace accumulators
} // namespace histogram
} // namespace boost

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED
namespace std {
template <class T, class U>
struct common_type<boost::histogram::accumulators::weighted_sum<T>,
                   boost::histogram::accumulators::weighted_sum<U>> {
  using type = boost::histogram::accumulators::weighted_sum<common_type_t<T, U>>;
};

template <class T, class U>
struct common_type<boost::histogram::accumulators::weighted_sum<T>, U> {
  using type = boost::histogram::accumulators::weighted_sum<common_type_t<T, U>>;
};

template <class T, class U>
struct common_type<T, boost::histogram::accumulators::weighted_sum<U>> {
  using type = boost::histogram::accumulators::weighted_sum<common_type_t<T, U>>;
};
} // namespace std
#endif

#endif
