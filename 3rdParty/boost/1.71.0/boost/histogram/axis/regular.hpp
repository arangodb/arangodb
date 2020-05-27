// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_AXIS_REGULAR_HPP
#define BOOST_HISTOGRAM_AXIS_REGULAR_HPP

#include <boost/assert.hpp>
#include <boost/histogram/axis/interval_view.hpp>
#include <boost/histogram/axis/iterator.hpp>
#include <boost/histogram/axis/option.hpp>
#include <boost/histogram/detail/compressed_pair.hpp>
#include <boost/histogram/detail/convert_integer.hpp>
#include <boost/histogram/detail/relaxed_equal.hpp>
#include <boost/histogram/detail/replace_default.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/throw_exception.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace histogram {
namespace detail {

template <class T>
using get_scale_type_helper = typename T::value_type;

template <class T>
using get_scale_type = mp11::mp_eval_or<T, detail::get_scale_type_helper, T>;

struct one_unit {};

template <class T>
T operator*(T&& t, const one_unit&) {
  return std::forward<T>(t);
}

template <class T>
T operator/(T&& t, const one_unit&) {
  return std::forward<T>(t);
}

template <class T>
using get_unit_type_helper = typename T::unit_type;

template <class T>
using get_unit_type = mp11::mp_eval_or<one_unit, detail::get_unit_type_helper, T>;

template <class T, class R = get_scale_type<T>>
R get_scale(const T& t) {
  return t / get_unit_type<T>();
}

} // namespace detail

namespace axis {

namespace transform {

/// Identity transform for equidistant bins.
struct id {
  /// Pass-through.
  template <typename T>
  static T forward(T&& x) noexcept {
    return std::forward<T>(x);
  }

  /// Pass-through.
  template <typename T>
  static T inverse(T&& x) noexcept {
    return std::forward<T>(x);
  }
};

/// Log transform for equidistant bins in log-space.
struct log {
  /// Returns log(x) of external value x.
  template <typename T>
  static T forward(T x) {
    return std::log(x);
  }

  /// Returns exp(x) for internal value x.
  template <typename T>
  static T inverse(T x) {
    return std::exp(x);
  }
};

/// Sqrt transform for equidistant bins in sqrt-space.
struct sqrt {
  /// Returns sqrt(x) of external value x.
  template <typename T>
  static T forward(T x) {
    return std::sqrt(x);
  }

  /// Returns x^2 of internal value x.
  template <typename T>
  static T inverse(T x) {
    return x * x;
  }
};

/// Pow transform for equidistant bins in pow-space.
struct pow {
  double power = 1; /**< power index */

  /// Make transform with index p.
  explicit pow(double p) : power(p) {}
  pow() = default;

  /// Returns pow(x, power) of external value x.
  template <typename T>
  auto forward(T x) const {
    return std::pow(x, power);
  }

  /// Returns pow(x, 1/power) of external value x.
  template <typename T>
  auto inverse(T x) const {
    return std::pow(x, 1.0 / power);
  }

  bool operator==(const pow& o) const noexcept { return power == o.power; }
};

} // namespace transform

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED
// Type envelope to mark value as step size
template <typename T>
struct step_type {
  T value;
};
#endif

/**
  Helper function to mark argument as step size.
 */
template <typename T>
auto step(T&& t) {
  return step_type<T&&>{std::forward<T>(t)};
}

/**
  Axis for equidistant intervals on the real line.

  The most common binning strategy. Very fast. Binning is a O(1) operation.

  @tparam Value input value type, must be floating point.
  @tparam Transform builtin or user-defined transform type.
  @tparam MetaData type to store meta data.
  @tparam Options see boost::histogram::axis::option (all values allowed).
 */
template <class Value, class Transform, class MetaData, class Options>
class regular : public iterator_mixin<regular<Value, Transform, MetaData, Options>>,
                protected detail::replace_default<Transform, transform::id> {
  using value_type = Value;
  using transform_type = detail::replace_default<Transform, transform::id>;
  using metadata_type = detail::replace_default<MetaData, std::string>;
  using options_type =
      detail::replace_default<Options, decltype(option::underflow | option::overflow)>;

  using unit_type = detail::get_unit_type<value_type>;
  using internal_value_type = detail::get_scale_type<value_type>;
  static_assert(std::is_floating_point<internal_value_type>::value,
                "variable axis requires floating point type");

public:
  constexpr regular() = default;
  regular(const regular&) = default;
  regular& operator=(const regular&) = default;
  regular(regular&& o) noexcept
      : transform_type(std::move(o))
      , size_meta_(std::move(o.size_meta_))
      , min_(o.min_)
      , delta_(o.delta_) {
    static_assert(std::is_nothrow_move_constructible<transform_type>::value, "");
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_constructible<metadata_type>::value,
                  "");
  }
  regular& operator=(regular&& o) noexcept {
    static_assert(std::is_nothrow_move_assignable<transform_type>::value, "");
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_assignable<metadata_type>::value,
                  "");
    transform_type::operator=(std::move(o));
    size_meta_ = std::move(o.size_meta_);
    min_ = o.min_;
    delta_ = o.delta_;
    return *this;
  }

  /** Construct n bins over real transformed range [start, stop).
   *
   * @param trans    transform instance to use.
   * @param n        number of bins.
   * @param start    low edge of first bin.
   * @param stop     high edge of last bin.
   * @param meta     description of the axis (optional).
   */
  regular(transform_type trans, unsigned n, value_type start, value_type stop,
          metadata_type meta = {})
      : transform_type(std::move(trans))
      , size_meta_(static_cast<index_type>(n), std::move(meta))
      , min_(this->forward(detail::get_scale(start)))
      , delta_(this->forward(detail::get_scale(stop)) - min_) {
    if (size() == 0) BOOST_THROW_EXCEPTION(std::invalid_argument("bins > 0 required"));
    if (!std::isfinite(min_) || !std::isfinite(delta_))
      BOOST_THROW_EXCEPTION(
          std::invalid_argument("forward transform of start or stop invalid"));
    if (delta_ == 0)
      BOOST_THROW_EXCEPTION(std::invalid_argument("range of axis is zero"));
  }

  /** Construct n bins over real range [start, stop).
   *
   * @param n        number of bins.
   * @param start    low edge of first bin.
   * @param stop     high edge of last bin.
   * @param meta     description of the axis (optional).
   */
  regular(unsigned n, value_type start, value_type stop, metadata_type meta = {})
      : regular({}, n, start, stop, std::move(meta)) {}

  /** Construct bins with the given step size over real transformed range
   * [start, stop).
   *
   * @param trans   transform instance to use.
   * @param step    width of a single bin.
   * @param start   low edge of first bin.
   * @param stop    upper limit of high edge of last bin (see below).
   * @param meta    description of the axis (optional).
   *
   * The axis computes the number of bins as n = abs(stop - start) / step,
   * rounded down. This means that stop is an upper limit to the actual value
   * (start + n * step).
   */
  template <class T>
  regular(transform_type trans, const step_type<T>& step, value_type start,
          value_type stop, metadata_type meta = {})
      : regular(trans, static_cast<index_type>(std::abs(stop - start) / step.value),
                start,
                start + static_cast<index_type>(std::abs(stop - start) / step.value) *
                            step.value,
                std::move(meta)) {}

  /** Construct bins with the given step size over real range [start, stop).
   *
   * @param step    width of a single bin.
   * @param start   low edge of first bin.
   * @param stop    upper limit of high edge of last bin (see below).
   * @param meta    description of the axis (optional).
   *
   * The axis computes the number of bins as n = abs(stop - start) / step,
   * rounded down. This means that stop is an upper limit to the actual value
   * (start + n * step).
   */
  template <class T>
  regular(const step_type<T>& step, value_type start, value_type stop,
          metadata_type meta = {})
      : regular({}, step, start, stop, std::move(meta)) {}

  /// Constructor used by algorithm::reduce to shrink and rebin (not for users).
  regular(const regular& src, index_type begin, index_type end, unsigned merge)
      : regular(src.transform(), (end - begin) / merge, src.value(begin), src.value(end),
                src.metadata()) {
    BOOST_ASSERT((end - begin) % merge == 0);
    if (options_type::test(option::circular) && !(begin == 0 && end == src.size()))
      BOOST_THROW_EXCEPTION(std::invalid_argument("cannot shrink circular axis"));
  }

  /// Return instance of the transform type.
  const transform_type& transform() const noexcept { return *this; }

  /// Return index for value argument.
  index_type index(value_type x) const noexcept {
    // Runs in hot loop, please measure impact of changes
    auto z = (this->forward(x / unit_type{}) - min_) / delta_;
    if (options_type::test(option::circular)) {
      if (std::isfinite(z)) {
        z -= std::floor(z);
        return static_cast<index_type>(z * size());
      }
    } else {
      if (z < 1) {
        if (z >= 0)
          return static_cast<index_type>(z * size());
        else
          return -1;
      }
    }
    return size(); // also returned if x is NaN
  }

  /// Returns index and shift (if axis has grown) for the passed argument.
  auto update(value_type x) noexcept {
    BOOST_ASSERT(options_type::test(option::growth));
    const auto z = (this->forward(x / unit_type{}) - min_) / delta_;
    if (z < 1) { // don't use i here!
      if (z >= 0) {
        const auto i = static_cast<axis::index_type>(z * size());
        return std::make_pair(i, 0);
      }
      if (z != -std::numeric_limits<internal_value_type>::infinity()) {
        const auto stop = min_ + delta_;
        const auto i = static_cast<axis::index_type>(std::floor(z * size()));
        min_ += i * (delta_ / size());
        delta_ = stop - min_;
        size_meta_.first() -= i;
        return std::make_pair(0, -i);
      }
      // z is -infinity
      return std::make_pair(-1, 0);
    }
    // z either beyond range, infinite, or NaN
    if (z < std::numeric_limits<internal_value_type>::infinity()) {
      const auto i = static_cast<axis::index_type>(z * size());
      const auto n = i - size() + 1;
      delta_ /= size();
      delta_ *= size() + n;
      size_meta_.first() += n;
      return std::make_pair(i, -n);
    }
    // z either infinite or NaN
    return std::make_pair(size(), 0);
  }

  /// Return value for fractional index argument.
  value_type value(real_index_type i) const noexcept {
    auto z = i / size();
    if (!options_type::test(option::circular) && z < 0.0)
      z = -std::numeric_limits<internal_value_type>::infinity() * delta_;
    else if (options_type::test(option::circular) || z <= 1.0)
      z = (1.0 - z) * min_ + z * (min_ + delta_);
    else {
      z = std::numeric_limits<internal_value_type>::infinity() * delta_;
    }
    return static_cast<value_type>(this->inverse(z) * unit_type());
  }

  /// Return bin for index argument.
  decltype(auto) bin(index_type idx) const noexcept {
    return interval_view<regular>(*this, idx);
  }

  /// Returns the number of bins, without over- or underflow.
  index_type size() const noexcept { return size_meta_.first(); }
  /// Returns the options.
  static constexpr unsigned options() noexcept { return options_type::value; }
  /// Returns reference to metadata.
  metadata_type& metadata() noexcept { return size_meta_.second(); }
  /// Returns reference to const metadata.
  const metadata_type& metadata() const noexcept { return size_meta_.second(); }

  template <class V, class T, class M, class O>
  bool operator==(const regular<V, T, M, O>& o) const noexcept {
    return detail::relaxed_equal(transform(), o.transform()) && size() == o.size() &&
           detail::relaxed_equal(metadata(), o.metadata()) && min_ == o.min_ &&
           delta_ == o.delta_;
  }
  template <class V, class T, class M, class O>
  bool operator!=(const regular<V, T, M, O>& o) const noexcept {
    return !operator==(o);
  }

  template <class Archive>
  void serialize(Archive&, unsigned);

private:
  detail::compressed_pair<index_type, metadata_type> size_meta_{0};
  internal_value_type min_{0}, delta_{1};

  template <class V, class T, class M, class O>
  friend class regular;
};

#if __cpp_deduction_guides >= 201606

template <class T>
regular(unsigned, T, T)->regular<detail::convert_integer<T, double>>;

template <class T>
regular(unsigned, T, T, const char*)->regular<detail::convert_integer<T, double>>;

template <class T, class M>
regular(unsigned, T, T, M)->regular<detail::convert_integer<T, double>, transform::id, M>;

template <class Tr, class T>
regular(Tr, unsigned, T, T)->regular<detail::convert_integer<T, double>, Tr>;

template <class Tr, class T>
regular(Tr, unsigned, T, T, const char*)->regular<detail::convert_integer<T, double>, Tr>;

template <class Tr, class T, class M>
regular(Tr, unsigned, T, T, M)->regular<detail::convert_integer<T, double>, Tr, M>;

#endif

/// Regular axis with circular option already set.
template <class Value = double, class MetaData = use_default, class Options = use_default>
#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED
using circular = regular<Value, transform::id, MetaData,
                         decltype(detail::replace_default<Options, option::overflow_t>{} |
                                  option::circular)>;
#else
class circular;
#endif

} // namespace axis
} // namespace histogram
} // namespace boost

#endif
