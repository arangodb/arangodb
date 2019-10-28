// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ACCUMULATORS_WEIGHTED_MEAN_HPP
#define BOOST_HISTOGRAM_ACCUMULATORS_WEIGHTED_MEAN_HPP

#include <boost/histogram/fwd.hpp>
#include <type_traits>

namespace boost {
namespace histogram {
namespace accumulators {

/**
  Calculates mean and variance of weighted sample.

  Uses West's incremental algorithm to improve numerical stability
  of mean and variance computation.
*/
template <typename RealType>
class weighted_mean {
public:
  weighted_mean() = default;
  weighted_mean(const RealType& wsum, const RealType& wsum2, const RealType& mean,
                const RealType& variance)
      : sum_of_weights_(wsum)
      , sum_of_weights_squared_(wsum2)
      , weighted_mean_(mean)
      , sum_of_weighted_deltas_squared_(
            variance * (sum_of_weights_ - sum_of_weights_squared_ / sum_of_weights_)) {}

  void operator()(const RealType& x) { operator()(1, x); }

  void operator()(const RealType& w, const RealType& x) {
    sum_of_weights_ += w;
    sum_of_weights_squared_ += w * w;
    const auto delta = x - weighted_mean_;
    weighted_mean_ += w * delta / sum_of_weights_;
    sum_of_weighted_deltas_squared_ += w * delta * (x - weighted_mean_);
  }

  template <typename T>
  weighted_mean& operator+=(const weighted_mean<T>& rhs) {
    const auto tmp = weighted_mean_ * sum_of_weights_ +
                     static_cast<RealType>(rhs.weighted_mean_ * rhs.sum_of_weights_);
    sum_of_weights_ += static_cast<RealType>(rhs.sum_of_weights_);
    sum_of_weights_squared_ += static_cast<RealType>(rhs.sum_of_weights_squared_);
    weighted_mean_ = tmp / sum_of_weights_;
    sum_of_weighted_deltas_squared_ +=
        static_cast<RealType>(rhs.sum_of_weighted_deltas_squared_);
    return *this;
  }

  weighted_mean& operator*=(const RealType& s) {
    weighted_mean_ *= s;
    sum_of_weighted_deltas_squared_ *= s * s;
    return *this;
  }

  template <typename T>
  bool operator==(const weighted_mean<T>& rhs) const noexcept {
    return sum_of_weights_ == rhs.sum_of_weights_ &&
           sum_of_weights_squared_ == rhs.sum_of_weights_squared_ &&
           weighted_mean_ == rhs.weighted_mean_ &&
           sum_of_weighted_deltas_squared_ == rhs.sum_of_weighted_deltas_squared_;
  }

  template <typename T>
  bool operator!=(const T& rhs) const noexcept {
    return !operator==(rhs);
  }

  const RealType& sum_of_weights() const noexcept { return sum_of_weights_; }
  const RealType& value() const noexcept { return weighted_mean_; }
  RealType variance() const {
    return sum_of_weighted_deltas_squared_ /
           (sum_of_weights_ - sum_of_weights_squared_ / sum_of_weights_);
  }

  template <class Archive>
  void serialize(Archive&, unsigned /* version */);

private:
  RealType sum_of_weights_ = RealType(), sum_of_weights_squared_ = RealType(),
           weighted_mean_ = RealType(), sum_of_weighted_deltas_squared_ = RealType();
};

} // namespace accumulators
} // namespace histogram
} // namespace boost

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED
namespace std {
template <class T, class U>
/// Specialization for boost::histogram::accumulators::weighted_mean.
struct common_type<boost::histogram::accumulators::weighted_mean<T>,
                   boost::histogram::accumulators::weighted_mean<U>> {
  using type = boost::histogram::accumulators::weighted_mean<common_type_t<T, U>>;
};
} // namespace std
#endif

#endif
