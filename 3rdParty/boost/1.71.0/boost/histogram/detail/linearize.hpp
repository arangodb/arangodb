// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_LINEARIZE_HPP
#define BOOST_HISTOGRAM_DETAIL_LINEARIZE_HPP

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/histogram/axis/traits.hpp>
#include <boost/histogram/axis/variant.hpp>
#include <boost/histogram/detail/args_type.hpp>
#include <boost/histogram/detail/axes.hpp>
#include <boost/histogram/detail/make_default.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include <boost/histogram/detail/tuple_slice.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/throw_exception.hpp>
#include <mutex>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace boost {
namespace histogram {
namespace detail {

template <class T>
using has_underflow =
    decltype(axis::traits::static_options<T>::test(axis::option::underflow));

template <class T>
using is_growing = decltype(axis::traits::static_options<T>::test(axis::option::growth));

template <class T>
using is_multidim = is_tuple<std::decay_t<arg_type<decltype(&T::index)>>>;

template <template <class> class Trait, class T>
struct has_special_axis_impl : Trait<T> {};

template <template <class> class Trait, class... Ts>
struct has_special_axis_impl<Trait, std::tuple<Ts...>> : mp11::mp_or<Trait<Ts>...> {};

template <template <class> class Trait, class... Ts>
struct has_special_axis_impl<Trait, axis::variant<Ts...>> : mp11::mp_or<Trait<Ts>...> {};

template <template <class> class Trait, class T>
using has_special_axis =
    has_special_axis_impl<Trait, mp11::mp_if<is_vector_like<T>, mp11::mp_first<T>, T>>;

template <class T>
using has_multidim_axis = has_special_axis<is_multidim, T>;

template <class T>
using has_growing_axis = has_special_axis<is_growing, T>;

/// Index with an invalid state
struct optional_index {
  std::size_t idx = 0;
  std::size_t stride = 1;
  operator bool() const { return stride > 0; }
  std::size_t operator*() const { return idx; }
};

// no underflow, no overflow
inline void linearize(std::false_type, std::false_type, optional_index& out,
                      const axis::index_type size, const axis::index_type i) noexcept {
  out.idx += i * out.stride;
  out.stride *= i >= 0 && i < size ? size : 0;
}

// no underflow, overflow
inline void linearize(std::false_type, std::true_type, optional_index& out,
                      const axis::index_type size, const axis::index_type i) noexcept {
  BOOST_ASSERT(i <= size);
  out.idx += i * out.stride;
  out.stride *= i >= 0 ? size + 1 : 0;
}

// underflow, no overflow
inline void linearize(std::true_type, std::false_type, optional_index& out,
                      const axis::index_type size, const axis::index_type i) noexcept {
  // internal index must be shifted by +1 since axis has underflow bin
  BOOST_ASSERT(i + 1 >= 0);
  out.idx += (i + 1) * out.stride;
  out.stride *= i < size ? size + 1 : 0;
}

// underflow, overflow
inline void linearize(std::true_type, std::true_type, optional_index& out,
                      const axis::index_type size, const axis::index_type i) noexcept {
  // internal index must be shifted by +1 since axis has underflow bin
  BOOST_ASSERT(i + 1 >= 0);
  BOOST_ASSERT(i <= size);
  out.idx += (i + 1) * out.stride;
  out.stride *= size + 2;
}

template <class Axis, class Value>
void linearize_value(optional_index& o, const Axis& a, const Value& v) {
  using O = axis::traits::static_options<Axis>;
  linearize(O::test(axis::option::underflow), O::test(axis::option::overflow), o,
            a.size(), axis::traits::index(a, v));
}

template <class... Ts, class Value>
void linearize_value(optional_index& o, const axis::variant<Ts...>& a, const Value& v) {
  axis::visit([&o, &v](const auto& a) { linearize_value(o, a, v); }, a);
}

template <class Axis, class Value>
axis::index_type linearize_value_growth(optional_index& o, Axis& a, const Value& v) {
  using O = axis::traits::static_options<Axis>;
  axis::index_type i, s;
  std::tie(i, s) = axis::traits::update(a, v);
  linearize(O::test(axis::option::underflow), O::test(axis::option::overflow), o,
            a.size(), i);
  return s;
}

template <class... Ts, class Value>
axis::index_type linearize_value_growth(optional_index& o, axis::variant<Ts...>& a,
                                        const Value& v) {
  return axis::visit([&o, &v](auto& a) { return linearize_value_growth(o, a, v); }, a);
}

template <class A>
void linearize_index(optional_index& out, const A& axis, const axis::index_type i) {
  // A may be axis or variant, cannot use static option detection here
  const auto opt = axis::traits::options(axis);
  const auto shift = opt & axis::option::underflow ? 1 : 0;
  const auto extent = axis.size() + (opt & axis::option::overflow ? 1 : 0) + shift;
  // i may be arbitrarily out of range
  linearize(std::false_type{}, std::false_type{}, out, extent, i + shift);
}

template <class S, class A>
void grow_storage(const A& axes, S& storage, const axis::index_type* shifts) {
  struct item {
    axis::index_type idx, old_extent;
    std::size_t new_stride;
  } data[buffer_size<A>::value];
  const auto* sit = shifts;
  auto dit = data;
  std::size_t s = 1;
  for_each_axis(axes, [&](const auto& a) {
    const auto n = axis::traits::extent(a);
    *dit++ = {0, n - std::abs(*sit++), s};
    s *= n;
  });
  auto new_storage = make_default(storage);
  new_storage.reset(bincount(axes));
  const auto dlast = data + axes_rank(axes) - 1;
  for (const auto& x : storage) {
    auto ns = new_storage.begin();
    sit = shifts;
    dit = data;
    for_each_axis(axes, [&](const auto& a) {
      using opt = axis::traits::static_options<decltype(a)>;
      if (opt::test(axis::option::underflow)) {
        if (dit->idx == 0) {
          // axis has underflow and we are in the underflow bin:
          // keep storage pointer unchanged
          ++dit;
          ++sit;
          return;
        }
      }
      if (opt::test(axis::option::overflow)) {
        if (dit->idx == dit->old_extent - 1) {
          // axis has overflow and we are in the overflow bin:
          // move storage pointer to corresponding overflow bin position
          ns += (axis::traits::extent(a) - 1) * dit->new_stride;
          ++dit;
          ++sit;
          return;
        }
      }
      // we are in a normal bin:
      // move storage pointer to index position, apply positive shifts
      ns += (dit->idx + std::max(*sit, 0)) * dit->new_stride;
      ++dit;
      ++sit;
    });
    // assign old value to new location
    *ns = x;
    // advance multi-dimensional index
    dit = data;
    ++dit->idx;
    while (dit != dlast && dit->idx == dit->old_extent) {
      dit->idx = 0;
      ++(++dit)->idx;
    }
  }
  storage = std::move(new_storage);
}

// histogram has no growing and no multidim axis, axis rank known at compile-time
template <class S, class... As, class... Us>
optional_index index(std::false_type, std::false_type, const std::tuple<As...>& axes, S&,
                     const std::tuple<Us...>& args) {
  optional_index idx;
  static_assert(sizeof...(As) == sizeof...(Us), "number of arguments != histogram rank");
  mp11::mp_for_each<mp11::mp_iota_c<sizeof...(As)>>(
      [&](auto i) { linearize_value(idx, axis_get<i>(axes), std::get<i>(args)); });
  return idx;
}

// histogram has no growing and no multidim axis, axis rank known at run-time
template <class A, class S, class U>
optional_index index(std::false_type, std::false_type, const A& axes, S&, const U& args) {
  constexpr auto nargs = static_cast<unsigned>(std::tuple_size<U>::value);
  optional_index idx;
  if (axes_rank(axes) != nargs)
    BOOST_THROW_EXCEPTION(std::invalid_argument("number of arguments != histogram rank"));
  constexpr auto nbuf = buffer_size<A>::value;
  mp11::mp_for_each<mp11::mp_iota_c<(nargs < nbuf ? nargs : nbuf)>>(
      [&](auto i) { linearize_value(idx, axis_get<i>(axes), std::get<i>(args)); });
  return idx;
}

// special case: if histogram::operator()(tuple(1, 2)) is called on 1d histogram
// with axis that accepts 2d tuple, this should not fail
// - solution is to forward tuples of size > 1 directly to axis for 1d
// histograms
// - has nice side-effect of making histogram::operator(1, 2) work as well
// - cannot detect call signature of axis at compile-time in all configurations
//   (axis::variant provides generic call interface and hides concrete
//   interface), so we throw at runtime if incompatible argument is passed (e.g.
//   3d tuple)

// histogram has no growing multidim axis, axis rank == 1 known at compile-time
template <class S, class A, class U>
optional_index index(std::false_type, std::true_type, const std::tuple<A>& axes, S&,
                     const U& args) {
  optional_index idx;
  linearize_value(idx, axis_get<0>(axes), args);
  return idx;
}

// histogram has no growing multidim axis, axis rank > 1 known at compile-time
template <class S, class U, class A0, class A1, class... As>
optional_index index(std::false_type, std::true_type,
                     const std::tuple<A0, A1, As...>& axes, S& s, const U& args) {
  return index(std::false_type{}, std::false_type{}, axes, s, args);
}

// histogram has no growing multidim axis, axis rank known at run-time
template <class A, class S, class U>
optional_index index(std::false_type, std::true_type, const A& axes, S&, const U& args) {
  optional_index idx;
  const auto rank = axes_rank(axes);
  constexpr auto nargs = static_cast<unsigned>(std::tuple_size<U>::value);
  if (rank == 1 && nargs > 1)
    linearize_value(idx, axis_get<0>(axes), args);
  else {
    if (rank != nargs)
      BOOST_THROW_EXCEPTION(
          std::invalid_argument("number of arguments != histogram rank"));
    constexpr auto nbuf = buffer_size<A>::value;
    mp11::mp_for_each<mp11::mp_iota_c<(nargs < nbuf ? nargs : nbuf)>>(
        [&](auto i) { linearize_value(idx, axis_get<i>(axes), std::get<i>(args)); });
  }
  return idx;
}

// histogram has growing axis
template <class B, class T, class S, class U>
optional_index index(std::true_type, B, T& axes, S& storage, const U& args) {
  optional_index idx;
  constexpr unsigned nbuf = buffer_size<T>::value;
  axis::index_type shifts[nbuf];
  const auto rank = axes_rank(axes);
  constexpr auto nargs = static_cast<unsigned>(std::tuple_size<U>::value);

  bool update_needed = false;
  if (rank == 1 && nargs > 1) {
    shifts[0] = linearize_value_growth(idx, axis_get<0>(axes), args);
    update_needed = (shifts[0] != 0);
  } else {
    if (rank != nargs)
      BOOST_THROW_EXCEPTION(
          std::invalid_argument("number of arguments != histogram rank"));
    mp11::mp_for_each<mp11::mp_iota_c<(nargs < nbuf ? nargs : nbuf)>>([&](auto i) {
      shifts[i] = linearize_value_growth(idx, axis_get<i>(axes), std::get<i>(args));
      update_needed |= (shifts[i] != 0);
    });
  }

  if (update_needed) grow_storage(axes, storage, shifts);
  return idx;
}

template <class U>
constexpr auto weight_sample_indices() noexcept {
  if (is_weight<U>::value) return std::make_pair(0, -1);
  if (is_sample<U>::value) return std::make_pair(-1, 0);
  return std::make_pair(-1, -1);
}

template <class U0, class U1, class... Us>
constexpr auto weight_sample_indices() noexcept {
  using L = mp11::mp_list<U0, U1, Us...>;
  const int n = sizeof...(Us) + 1;
  if (is_weight<mp11::mp_at_c<L, 0>>::value) {
    if (is_sample<mp11::mp_at_c<L, 1>>::value) return std::make_pair(0, 1);
    if (is_sample<mp11::mp_at_c<L, n>>::value) return std::make_pair(0, n);
    return std::make_pair(0, -1);
  }
  if (is_sample<mp11::mp_at_c<L, 0>>::value) {
    if (is_weight<mp11::mp_at_c<L, 1>>::value) return std::make_pair(1, 0);
    if (is_weight<mp11::mp_at_c<L, n>>::value) return std::make_pair(n, 0);
    return std::make_pair(-1, 0);
  }
  if (is_weight<mp11::mp_at_c<L, n>>::value) {
    // 0, n already covered
    if (is_sample<mp11::mp_at_c<L, (n - 1)>>::value) return std::make_pair(n, n - 1);
    return std::make_pair(n, -1);
  }
  if (is_sample<mp11::mp_at_c<L, n>>::value) {
    // n, 0 already covered
    if (is_weight<mp11::mp_at_c<L, (n - 1)>>::value) return std::make_pair(n - 1, n);
    return std::make_pair(-1, n);
  }
  return std::make_pair(-1, -1);
}

template <class T, class U>
void fill_impl(mp11::mp_int<-1>, mp11::mp_int<-1>, std::false_type, T&& t,
               const U&) noexcept {
  t();
}

template <class T, class U>
void fill_impl(mp11::mp_int<-1>, mp11::mp_int<-1>, std::true_type, T&& t,
               const U&) noexcept {
  ++t;
}

template <class IW, class T, class U>
void fill_impl(IW, mp11::mp_int<-1>, std::false_type, T&& t, const U& u) noexcept {
  t(std::get<IW::value>(u).value);
}

template <class IW, class T, class U>
void fill_impl(IW, mp11::mp_int<-1>, std::true_type, T&& t, const U& u) noexcept {
  t += std::get<IW::value>(u).value;
}

template <class IS, class T, class U>
void fill_impl(mp11::mp_int<-1>, IS, std::false_type, T&& t, const U& u) noexcept {
  mp11::tuple_apply([&t](auto&&... args) { t(args...); }, std::get<IS::value>(u).value);
}

template <class IW, class IS, class T, class U>
void fill_impl(IW, IS, std::false_type, T&& t, const U& u) noexcept {
  mp11::tuple_apply([&](auto&&... args2) { t(std::get<IW::value>(u).value, args2...); },
                    std::get<IS::value>(u).value);
}

template <class A, class S, class... Us>
typename S::iterator fill(A& axes, S& storage, const std::tuple<Us...>& tus) {
  constexpr auto iws = weight_sample_indices<Us...>();
  constexpr unsigned n = sizeof...(Us) - (iws.first > -1) - (iws.second > -1);
  constexpr unsigned i = (iws.first == 0 || iws.second == 0)
                             ? (iws.first == 1 || iws.second == 1 ? 2 : 1)
                             : 0;
  const auto idx = index(has_growing_axis<A>{}, has_multidim_axis<A>{}, axes, storage,
                         tuple_slice<i, n>(tus));
  if (idx) {
    fill_impl(mp11::mp_int<iws.first>{}, mp11::mp_int<iws.second>{},
              has_operator_preincrement<typename S::value_type>{}, storage[*idx], tus);
    return storage.begin() + *idx;
  }
  return storage.end();
}

template <class A, class... Us>
optional_index at(const A& axes, const std::tuple<Us...>& args) {
  if (axes_rank(axes) != sizeof...(Us))
    BOOST_THROW_EXCEPTION(std::invalid_argument("number of arguments != histogram rank"));
  optional_index idx;
  mp11::mp_for_each<mp11::mp_iota_c<sizeof...(Us)>>([&](auto i) {
    // axes_get works with static and dynamic axes
    linearize_index(idx, axis_get<i>(axes),
                    static_cast<axis::index_type>(std::get<i>(args)));
  });
  return idx;
}

template <class A, class U>
optional_index at(const A& axes, const U& args) {
  if (axes_rank(axes) != axes_rank(args))
    BOOST_THROW_EXCEPTION(std::invalid_argument("number of arguments != histogram rank"));
  optional_index idx;
  using std::begin;
  auto it = begin(args);
  for_each_axis(axes, [&](const auto& a) {
    linearize_index(idx, a, static_cast<axis::index_type>(*it++));
  });
  return idx;
}

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
