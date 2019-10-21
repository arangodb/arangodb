// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_AXIS_CATEGORY_HPP
#define BOOST_HISTOGRAM_AXIS_CATEGORY_HPP

#include <algorithm>
#include <boost/histogram/axis/iterator.hpp>
#include <boost/histogram/axis/option.hpp>
#include <boost/histogram/detail/compressed_pair.hpp>
#include <boost/histogram/detail/detect.hpp>
#include <boost/histogram/detail/relaxed_equal.hpp>
#include <boost/histogram/detail/replace_default.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace histogram {
namespace axis {

/**
  Maps at a set of unique values to bin indices.

  The axis maps a set of values to bins, following the order of arguments in the
  constructor. The optional overflow bin for this axis counts input values that
  are not part of the set. Binning has O(N) complexity, but with a very small
  factor. For small N (the typical use case) it beats other kinds of lookup.

  @tparam Value input value type, must be equal-comparable.
  @tparam MetaData type to store meta data.
  @tparam Options see boost::histogram::axis::option.
  @tparam Allocator allocator to use for dynamic memory management.

  The options `underflow` and `circular` are not allowed. The options `growth`
  and `overflow` are mutually exclusive.
*/
template <class Value, class MetaData, class Options, class Allocator>
class category : public iterator_mixin<category<Value, MetaData, Options, Allocator>> {
  using value_type = Value;
  using metadata_type = detail::replace_default<MetaData, std::string>;
  using options_type = detail::replace_default<Options, option::overflow_t>;
  static_assert(!options_type::test(option::underflow),
                "category axis cannot have underflow");
  static_assert(!options_type::test(option::circular),
                "category axis cannot be circular");
  static_assert(!options_type::test(option::growth) ||
                    !options_type::test(option::overflow),
                "growing category axis cannot have overflow");
  using allocator_type = Allocator;
  using vector_type = std::vector<value_type, allocator_type>;

public:
  explicit category(allocator_type alloc = {}) : vec_meta_(vector_type(alloc)) {}
  category(const category&) = default;
  category& operator=(const category&) = default;
  category(category&& o) noexcept : vec_meta_(std::move(o.vec_meta_)) {
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_constructible<metadata_type>::value,
                  "");
  }
  category& operator=(category&& o) noexcept {
    // std::string explicitly guarantees nothrow only in C++17
    static_assert(std::is_same<metadata_type, std::string>::value ||
                      std::is_nothrow_move_assignable<metadata_type>::value,
                  "");
    vec_meta_ = std::move(o.vec_meta_);
    return *this;
  }

  /** Construct from iterator range of unique values.
   *
   * \param begin     begin of category range of unique values.
   * \param end       end of category range of unique values.
   * \param meta      description of the axis.
   * \param alloc     allocator instance to use.
   */
  template <class It, class = detail::requires_iterator<It>>
  category(It begin, It end, metadata_type meta = {}, allocator_type alloc = {})
      : vec_meta_(vector_type(begin, end, alloc), std::move(meta)) {
    if (size() == 0) BOOST_THROW_EXCEPTION(std::invalid_argument("bins > 0 required"));
  }

  /** Construct axis from iterable sequence of unique values.
   *
   * \param iterable sequence of unique values.
   * \param meta     description of the axis.
   * \param alloc    allocator instance to use.
   */
  template <class C, class = detail::requires_iterable<C>>
  category(const C& iterable, metadata_type meta = {}, allocator_type alloc = {})
      : category(std::begin(iterable), std::end(iterable), std::move(meta),
                 std::move(alloc)) {}

  /** Construct axis from an initializer list of unique values.
   *
   * \param list   `std::initializer_list` of unique values.
   * \param meta   description of the axis.
   * \param alloc  allocator instance to use.
   */
  template <class U>
  category(std::initializer_list<U> list, metadata_type meta = {},
           allocator_type alloc = {})
      : category(list.begin(), list.end(), std::move(meta), std::move(alloc)) {}

  /// Return index for value argument.
  index_type index(const value_type& x) const noexcept {
    const auto beg = vec_meta_.first().begin();
    const auto end = vec_meta_.first().end();
    return static_cast<index_type>(std::distance(beg, std::find(beg, end, x)));
  }

  /// Returns index and shift (if axis has grown) for the passed argument.
  auto update(const value_type& x) {
    const auto i = index(x);
    if (i < size()) return std::make_pair(i, 0);
    vec_meta_.first().emplace_back(x);
    return std::make_pair(i, -1);
  }

  /// Return value for index argument.
  /// Throws `std::out_of_range` if the index is out of bounds.
  decltype(auto) value(index_type idx) const {
    if (idx < 0 || idx >= size())
      BOOST_THROW_EXCEPTION(std::out_of_range("category index out of range"));
    return vec_meta_.first()[idx];
  }

  /// Return value for index argument.
  decltype(auto) bin(index_type idx) const noexcept { return value(idx); }

  /// Returns the number of bins, without over- or underflow.
  index_type size() const noexcept {
    return static_cast<index_type>(vec_meta_.first().size());
  }
  /// Returns the options.
  static constexpr unsigned options() noexcept { return options_type::value; }
  /// Returns reference to metadata.
  metadata_type& metadata() noexcept { return vec_meta_.second(); }
  /// Returns reference to const metadata.
  const metadata_type& metadata() const noexcept { return vec_meta_.second(); }

  template <class V, class M, class O, class A>
  bool operator==(const category<V, M, O, A>& o) const noexcept {
    const auto& a = vec_meta_.first();
    const auto& b = o.vec_meta_.first();
    return std::equal(a.begin(), a.end(), b.begin(), b.end()) &&
           detail::relaxed_equal(metadata(), o.metadata());
  }

  template <class V, class M, class O, class A>
  bool operator!=(const category<V, M, O, A>& o) const noexcept {
    return !operator==(o);
  }

  auto get_allocator() const { return vec_meta_.first().get_allocator(); }

  template <class Archive>
  void serialize(Archive&, unsigned);

private:
  detail::compressed_pair<vector_type, metadata_type> vec_meta_;

  template <class V, class M, class O, class A>
  friend class category;
};

#if __cpp_deduction_guides >= 201606

template <class T>
category(std::initializer_list<T>)->category<T>;

category(std::initializer_list<const char*>)->category<std::string>;

template <class T>
category(std::initializer_list<T>, const char*)->category<T>;

template <class T, class M>
category(std::initializer_list<T>, const M&)->category<T, M>;

#endif

} // namespace axis
} // namespace histogram
} // namespace boost

#endif
