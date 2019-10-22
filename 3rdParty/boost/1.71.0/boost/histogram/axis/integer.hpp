// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_AXIS_INTEGER_HPP
#define BOOST_HISTOGRAM_AXIS_INTEGER_HPP

#include <boost/histogram/axis/iterator.hpp>
#include <boost/histogram/axis/option.hpp>
#include <boost/histogram/detail/compressed_pair.hpp>
#include <boost/histogram/detail/convert_integer.hpp>
#include <boost/histogram/detail/limits.hpp>
#include <boost/histogram/detail/relaxed_equal.hpp>
#include <boost/histogram/detail/replace_default.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/throw_exception.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace histogram {
namespace axis {

/**
  Axis for an interval of integer values with unit steps.

  Binning is a O(1) operation. This axis bins faster than a regular axis.

  @tparam Value input value type. Must be integer or floating point.
  @tparam MetaData type to store meta data.
  @tparam Options see boost::histogram::axis::option (all values allowed).
 */
template <class Value, class MetaData, class Options>
class integer : public iterator_mixin<integer<Value, MetaData, Options>> {
  static_assert(std::is_integral<Value>::value || std::is_floating_point<Value>::value,
                "integer axis requires type floating point or integral type");

  using value_type = Value;
  using local_index_type = std::conditional_t<std::is_integral<value_type>::value,
                                              index_type, real_index_type>;

  using metadata_type = detail::replace_default<MetaData, std::string>;
  using options_type =
      detail::replace_default<Options, decltype(option::underflow | option::overflow)>;

  static_assert(!options_type::test(option::circular) ||
                    std::is_floating_point<value_type>::value ||
                    !options_type::test(option::overflow),
                "integer axis with integral type cannot have overflow");

public:
  constexpr integer() = default;
  integer(const integer&) = default;
  integer& operator=(const integer&) = default;
  integer(integer&& o) noexcept : size_meta_(std::move(o.size_meta_)), min_(o.min_) {
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_constructible<metadata_type>::value,
                  "");
  }
  integer& operator=(integer&& o) noexcept {
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_assignable<metadata_type>::value,
                  "");
    size_meta_ = std::move(o.size_meta_);
    min_ = o.min_;
    return *this;
  }

  /** Construct over semi-open integer interval [start, stop).
   *
   * \param start    first integer of covered range.
   * \param stop     one past last integer of covered range.
   * \param meta     description of the axis.
   */
  integer(value_type start, value_type stop, metadata_type meta = {})
      : size_meta_(static_cast<index_type>(stop - start), std::move(meta)), min_(start) {
    if (stop <= start) BOOST_THROW_EXCEPTION(std::invalid_argument("bins > 0 required"));
  }

  /// Constructor used by algorithm::reduce to shrink and rebin.
  integer(const integer& src, index_type begin, index_type end, unsigned merge)
      : integer(src.value(begin), src.value(end), src.metadata()) {
    if (merge > 1)
      BOOST_THROW_EXCEPTION(std::invalid_argument("cannot merge bins for integer axis"));
    if (options_type::test(option::circular) && !(begin == 0 && end == src.size()))
      BOOST_THROW_EXCEPTION(std::invalid_argument("cannot shrink circular axis"));
  }

  /// Return index for value argument.
  index_type index(value_type x) const noexcept {
    return index_impl(std::is_floating_point<value_type>(), x);
  }

  /// Returns index and shift (if axis has grown) for the passed argument.
  auto update(value_type x) noexcept {
    auto impl = [this](long x) {
      const auto i = x - min_;
      if (i >= 0) {
        const auto k = static_cast<axis::index_type>(i);
        if (k < size()) return std::make_pair(k, 0);
        const auto n = k - size() + 1;
        size_meta_.first() += n;
        return std::make_pair(k, -n);
      }
      const auto k = static_cast<axis::index_type>(
          detail::static_if<std::is_floating_point<value_type>>(
              [](auto x) { return std::floor(x); }, [](auto x) { return x; }, i));
      min_ += k;
      size_meta_.first() -= k;
      return std::make_pair(0, -k);
    };

    return detail::static_if<std::is_floating_point<value_type>>(
        [this, impl](auto x) {
          if (std::isfinite(x)) return impl(static_cast<long>(std::floor(x)));
          // this->size() is workaround for gcc-5 bug
          return std::make_pair(x < 0 ? -1 : this->size(), 0);
        },
        impl, x);
  }

  /// Return value for index argument.
  value_type value(local_index_type i) const noexcept {
    if (!options_type::test(option::circular)) {
      if (i < 0) return detail::lowest<value_type>();
      if (i > size()) { return detail::highest<value_type>(); }
    }
    return min_ + i;
  }

  /// Return bin for index argument.
  decltype(auto) bin(index_type idx) const noexcept {
    return detail::static_if<std::is_floating_point<local_index_type>>(
        [this](auto idx) { return interval_view<integer>(*this, idx); },
        [this](auto idx) { return this->value(idx); }, idx);
  }

  /// Returns the number of bins, without over- or underflow.
  index_type size() const noexcept { return size_meta_.first(); }
  /// Returns the options.
  static constexpr unsigned options() noexcept { return options_type::value; }
  /// Returns reference to metadata.
  metadata_type& metadata() noexcept { return size_meta_.second(); }
  /// Returns reference to const metadata.
  const metadata_type& metadata() const noexcept { return size_meta_.second(); }

  template <class V, class M, class O>
  bool operator==(const integer<V, M, O>& o) const noexcept {
    return size() == o.size() && detail::relaxed_equal(metadata(), o.metadata()) &&
           min_ == o.min_;
  }

  template <class V, class M, class O>
  bool operator!=(const integer<V, M, O>& o) const noexcept {
    return !operator==(o);
  }

  template <class Archive>
  void serialize(Archive&, unsigned);

private:
  index_type index_impl(std::false_type, int x) const noexcept {
    const auto z = x - min_;
    if (options_type::test(option::circular))
      return static_cast<index_type>(z - std::floor(float(z) / size()) * size());
    if (z < size()) return z >= 0 ? z : -1;
    return size();
  }

  template <typename T>
  index_type index_impl(std::true_type, T x) const noexcept {
    // need to handle NaN, cannot simply cast to int and call int-implementation
    const auto z = x - min_;
    if (options_type::test(option::circular)) {
      if (std::isfinite(z))
        return static_cast<index_type>(std::floor(z) - std::floor(z / size()) * size());
    } else if (z < size()) {
      return z >= 0 ? static_cast<index_type>(z) : -1;
    }
    return size();
  }

  detail::compressed_pair<index_type, metadata_type> size_meta_{0};
  value_type min_{0};

  template <class V, class M, class O>
  friend class integer;
};

#if __cpp_deduction_guides >= 201606

template <class T>
integer(T, T)->integer<detail::convert_integer<T, index_type>>;

template <class T>
integer(T, T, const char*)->integer<detail::convert_integer<T, index_type>>;

template <class T, class M>
integer(T, T, M)->integer<detail::convert_integer<T, index_type>, M>;

#endif

} // namespace axis
} // namespace histogram
} // namespace boost

#endif
