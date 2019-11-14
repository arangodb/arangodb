// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_AXIS_VARIABLE_HPP
#define BOOST_HISTOGRAM_AXIS_VARIABLE_HPP

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/histogram/axis/interval_view.hpp>
#include <boost/histogram/axis/iterator.hpp>
#include <boost/histogram/axis/option.hpp>
#include <boost/histogram/detail/compressed_pair.hpp>
#include <boost/histogram/detail/convert_integer.hpp>
#include <boost/histogram/detail/detect.hpp>
#include <boost/histogram/detail/limits.hpp>
#include <boost/histogram/detail/relaxed_equal.hpp>
#include <boost/histogram/detail/replace_default.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/throw_exception.hpp>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace histogram {
namespace axis {

/**
  Axis for non-equidistant bins on the real line.

  Binning is a O(log(N)) operation. If speed matters and the problem domain
  allows it, prefer a regular axis, possibly with a transform.

  @tparam Value input value type, must be floating point.
  @tparam MetaData type to store meta data.
  @tparam Options see boost::histogram::axis::option (all values allowed).
  @tparam Allocator allocator to use for dynamic memory management.
 */
template <class Value, class MetaData, class Options, class Allocator>
class variable : public iterator_mixin<variable<Value, MetaData, Options, Allocator>> {
  static_assert(std::is_floating_point<Value>::value,
                "variable axis requires floating point type");

  using value_type = Value;
  using metadata_type = detail::replace_default<MetaData, std::string>;
  using options_type =
      detail::replace_default<Options, decltype(option::underflow | option::overflow)>;
  using allocator_type = Allocator;
  using vec_type = std::vector<Value, allocator_type>;

public:
  explicit variable(allocator_type alloc = {}) : vec_meta_(vec_type{alloc}) {}
  variable(const variable&) = default;
  variable& operator=(const variable&) = default;
  variable(variable&& o) noexcept : vec_meta_(std::move(o.vec_meta_)) {
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_constructible<metadata_type>::value,
                  "");
  }
  variable& operator=(variable&& o) noexcept {
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_assignable<metadata_type>::value,
                  "");
    vec_meta_ = std::move(o.vec_meta_);
    return *this;
  }

  /** Construct from iterator range of bin edges.
   *
   * \param begin begin of edge sequence.
   * \param end   end of edge sequence.
   * \param meta  description of the axis.
   * \param alloc allocator instance to use.
   */
  template <class It, class = detail::requires_iterator<It>>
  variable(It begin, It end, metadata_type meta = {}, allocator_type alloc = {})
      : vec_meta_(vec_type(std::move(alloc)), std::move(meta)) {
    if (std::distance(begin, end) <= 1)
      BOOST_THROW_EXCEPTION(std::invalid_argument("bins > 0 required"));

    auto& v = vec_meta_.first();
    v.reserve(std::distance(begin, end));
    v.emplace_back(*begin++);
    while (begin != end) {
      if (*begin <= v.back())
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("input sequence must be strictly ascending"));
      v.emplace_back(*begin++);
    }
  }

  /** Construct variable axis from iterable range of bin edges.
   *
   * \param iterable iterable range of bin edges.
   * \param meta     description of the axis.
   * \param alloc    allocator instance to use.
   */
  template <class U, class = detail::requires_iterable<U>>
  variable(const U& iterable, metadata_type meta = {}, allocator_type alloc = {})
      : variable(std::begin(iterable), std::end(iterable), std::move(meta),
                 std::move(alloc)) {}

  /** Construct variable axis from initializer list of bin edges.
   *
   * @param list  `std::initializer_list` of bin edges.
   * @param meta  description of the axis.
   * @param alloc allocator instance to use.
   */
  template <class U>
  variable(std::initializer_list<U> list, metadata_type meta = {},
           allocator_type alloc = {})
      : variable(list.begin(), list.end(), std::move(meta), std::move(alloc)) {}

  /// Constructor used by algorithm::reduce to shrink and rebin (not for users).
  variable(const variable& src, index_type begin, index_type end, unsigned merge)
      : vec_meta_(vec_type(src.get_allocator()), src.metadata()) {
    BOOST_ASSERT((end - begin) % merge == 0);
    if (options_type::test(option::circular) && !(begin == 0 && end == src.size()))
      BOOST_THROW_EXCEPTION(std::invalid_argument("cannot shrink circular axis"));
    auto& vec = vec_meta_.first();
    vec.reserve((end - begin) / merge);
    const auto beg = src.vec_meta_.first().begin();
    for (index_type i = begin; i <= end; i += merge) vec.emplace_back(*(beg + i));
  }

  /// Return index for value argument.
  index_type index(value_type x) const noexcept {
    const auto& v = vec_meta_.first();
    if (options_type::test(option::circular)) {
      const auto a = v[0];
      const auto b = v[size()];
      x -= std::floor((x - a) / (b - a)) * (b - a);
    }
    return static_cast<index_type>(std::upper_bound(v.begin(), v.end(), x) - v.begin() -
                                   1);
  }

  auto update(value_type x) noexcept {
    const auto i = index(x);
    if (std::isfinite(x)) {
      auto& vec = vec_meta_.first();
      if (0 <= i) {
        if (i < size()) return std::make_pair(i, 0);
        const auto d = value(size()) - value(size() - 0.5);
        x = std::nextafter(x, std::numeric_limits<value_type>::max());
        x = std::max(x, vec.back() + d);
        vec.push_back(x);
        return std::make_pair(i, -1);
      }
      const auto d = value(0.5) - value(0);
      x = std::min(x, value(0) - d);
      vec.insert(vec.begin(), x);
      return std::make_pair(0, -i);
    }
    return std::make_pair(x < 0 ? -1 : size(), 0);
  }

  /// Return value for fractional index argument.
  value_type value(real_index_type i) const noexcept {
    const auto& v = vec_meta_.first();
    if (options_type::test(option::circular)) {
      auto shift = std::floor(i / size());
      i -= shift * size();
      double z;
      const auto k = static_cast<index_type>(std::modf(i, &z));
      const auto a = v[0];
      const auto b = v[size()];
      return (1.0 - z) * v[k] + z * v[k + 1] + shift * (b - a);
    }
    if (i < 0) return detail::lowest<value_type>();
    if (i == size()) return v.back();
    if (i > size()) return detail::highest<value_type>();
    const auto k = static_cast<index_type>(i); // precond: i >= 0
    const real_index_type z = i - k;
    return (1.0 - z) * v[k] + z * v[k + 1];
  }

  /// Return bin for index argument.
  auto bin(index_type idx) const noexcept { return interval_view<variable>(*this, idx); }

  /// Returns the number of bins, without over- or underflow.
  index_type size() const noexcept {
    return static_cast<index_type>(vec_meta_.first().size()) - 1;
  }
  /// Returns the options.
  static constexpr unsigned options() noexcept { return options_type::value; }
  /// Returns reference to metadata.
  metadata_type& metadata() noexcept { return vec_meta_.second(); }
  /// Returns reference to const metadata.
  const metadata_type& metadata() const noexcept { return vec_meta_.second(); }

  template <class V, class M, class O, class A>
  bool operator==(const variable<V, M, O, A>& o) const noexcept {
    const auto& a = vec_meta_.first();
    const auto& b = o.vec_meta_.first();
    return std::equal(a.begin(), a.end(), b.begin(), b.end()) &&
           detail::relaxed_equal(metadata(), o.metadata());
  }

  template <class V, class M, class O, class A>
  bool operator!=(const variable<V, M, O, A>& o) const noexcept {
    return !operator==(o);
  }

  /// Return allocator instance.
  auto get_allocator() const { return vec_meta_.first().get_allocator(); }

  template <class Archive>
  void serialize(Archive&, unsigned);

private:
  detail::compressed_pair<vec_type, metadata_type> vec_meta_;

  template <class V, class M, class O, class A>
  friend class variable;
};

#if __cpp_deduction_guides >= 201606

template <class U, class T = detail::convert_integer<U, double>>
variable(std::initializer_list<U>)->variable<T>;

template <class U, class T = detail::convert_integer<U, double>>
variable(std::initializer_list<U>, const char*)->variable<T>;

template <class U, class M, class T = detail::convert_integer<U, double>>
variable(std::initializer_list<U>, M)->variable<T, M>;

template <class Iterable,
          class T = detail::convert_integer<
              std::decay_t<decltype(*std::begin(std::declval<Iterable&>()))>, double>>
variable(Iterable)->variable<T>;

template <class Iterable,
          class T = detail::convert_integer<
              std::decay_t<decltype(*std::begin(std::declval<Iterable&>()))>, double>>
variable(Iterable, const char*)->variable<T>;

template <class Iterable, class M,
          class T = detail::convert_integer<
              std::decay_t<decltype(*std::begin(std::declval<Iterable&>()))>, double>>
variable(Iterable, M)->variable<T, M>;

#endif

} // namespace axis
} // namespace histogram
} // namespace boost

#endif
