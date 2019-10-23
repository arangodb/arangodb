// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_AXIS_TRAITS_HPP
#define BOOST_HISTOGRAM_AXIS_TRAITS_HPP

#include <boost/histogram/axis/option.hpp>
#include <boost/histogram/detail/args_type.hpp>
#include <boost/histogram/detail/cat.hpp>
#include <boost/histogram/detail/detect.hpp>
#include <boost/histogram/detail/static_if.hpp>
#include <boost/histogram/detail/try_cast.hpp>
#include <boost/histogram/detail/type_name.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/throw_exception.hpp>
#include <boost/variant2/variant.hpp>
#include <stdexcept>
#include <utility>

namespace boost {
namespace histogram {
namespace detail {

template <class T>
using static_options_impl = axis::option::bitset<T::options()>;

template <class I, class D, class A>
double value_method_switch_impl1(std::false_type, I&&, D&&, const A&) {
  // comma trick to make all compilers happy; some would complain about
  // unreachable code after the throw, others about a missing return
  return BOOST_THROW_EXCEPTION(
             std::runtime_error(cat(type_name<A>(), " has no value method"))),
         double{};
}

template <class I, class D, class A>
decltype(auto) value_method_switch_impl1(std::true_type, I&& i, D&& d, const A& a) {
  using T = arg_type<decltype(&A::value)>;
  return static_if<std::is_same<T, axis::index_type>>(std::forward<I>(i),
                                                      std::forward<D>(d), a);
}

template <class I, class D, class A>
decltype(auto) value_method_switch(I&& i, D&& d, const A& a) {
  return value_method_switch_impl1(has_method_value<A>{}, std::forward<I>(i),
                                   std::forward<D>(d), a);
}

static axis::null_type null_value;

struct variant_access {
  template <class T, class T0, class Variant>
  static auto get_if_impl(mp11::mp_list<T, T0>, Variant* v) noexcept {
    return variant2::get_if<T>(&(v->impl));
  }

  template <class T, class T0, class Variant>
  static auto get_if_impl(mp11::mp_list<T, T0*>, Variant* v) noexcept {
    auto tp = variant2::get_if<mp11::mp_if<std::is_const<T0>, const T*, T*>>(&(v->impl));
    return tp ? *tp : nullptr;
  }

  template <class T, class Variant>
  static auto get_if(Variant* v) noexcept {
    using T0 = mp11::mp_first<std::decay_t<Variant>>;
    return get_if_impl(mp11::mp_list<T, T0>{}, v);
  }

  template <class T0, class Visitor, class Variant>
  static decltype(auto) visit_impl(mp11::mp_identity<T0>, Visitor&& vis, Variant&& v) {
    return variant2::visit(std::forward<Visitor>(vis), v.impl);
  }

  template <class T0, class Visitor, class Variant>
  static decltype(auto) visit_impl(mp11::mp_identity<T0*>, Visitor&& vis, Variant&& v) {
    return variant2::visit(
        [&vis](auto&& x) -> decltype(auto) { return std::forward<Visitor>(vis)(*x); },
        v.impl);
  }

  template <class Visitor, class Variant>
  static decltype(auto) visit(Visitor&& vis, Variant&& v) {
    using T0 = mp11::mp_first<std::decay_t<Variant>>;
    return visit_impl(mp11::mp_identity<T0>{}, std::forward<Visitor>(vis),
                      std::forward<Variant>(v));
  }
};

} // namespace detail

namespace axis {
namespace traits {

/** Returns reference to metadata of an axis.

  If the expression x.metadata() for an axis instance `x` (maybe const) is valid, return
  the result. Otherwise, return a reference to a static instance of
  boost::histogram::axis::null_type.

  @param axis any axis instance
*/
template <class Axis>
decltype(auto) metadata(Axis&& axis) noexcept {
  return detail::static_if<detail::has_method_metadata<std::decay_t<Axis>>>(
      [](auto&& a) -> decltype(auto) { return a.metadata(); },
      [](auto &&) -> mp11::mp_if<std::is_const<std::remove_reference_t<Axis>>,
                                 axis::null_type const&, axis::null_type&> {
        return detail::null_value;
      },
      std::forward<Axis>(axis));
}

/** Get static axis options for axis type.

  Doxygen does not render this well. This is a meta-function, it accepts an axis
  type and represents its boost::histogram::axis::option::bitset.

  If Axis::options() is valid and constexpr, static_options is the corresponding
  option type. Otherwise, it is boost::histogram::axis::option::growth_t, if the
  axis has a method `update`, else boost::histogram::axis::option::none_t.

  @tparam Axis axis type
*/
template <class Axis>
#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED
using static_options =
    mp11::mp_eval_or<mp11::mp_if<detail::has_method_update<std::decay_t<Axis>>,
                                 option::growth_t, option::none_t>,
                     detail::static_options_impl, std::decay_t<Axis>>;
#else
struct static_options;
#endif

/** Returns axis options as unsigned integer.

  If axis.options() is valid, return the result. Otherwise, return
  boost::histogram::axis::traits::static_options<decltype(axis)>::value.

  @param axis any axis instance
*/
template <class Axis>
constexpr unsigned options(const Axis& axis) noexcept {
  // cannot reuse static_options here, must also work for axis::variant
  return detail::static_if<detail::has_method_options<Axis>>(
      [](const auto& a) { return a.options(); },
      [](const auto&) { return static_options<Axis>::value; }, axis);
}

/** Returns axis size plus any extra bins for under- and overflow.

  @param axis any axis instance
*/
template <class Axis>
constexpr index_type extent(const Axis& axis) noexcept {
  const auto opt = options(axis);
  return axis.size() + (opt & option::underflow ? 1 : 0) +
         (opt & option::overflow ? 1 : 0);
}

/** Returns axis value for index.

  If the axis has no `value` method, throw std::runtime_error. If the method exists and
  accepts a floating point index, pass the index and return the result. If the method
  exists but accepts only integer indices, cast the floating point index to int, pass this
  index and return the result.

  @param axis any axis instance
  @param index floating point axis index
*/
template <class Axis>
decltype(auto) value(const Axis& axis, real_index_type index) {
  return detail::value_method_switch(
      [index](const auto& a) { return a.value(static_cast<index_type>(index)); },
      [index](const auto& a) { return a.value(index); }, axis);
}

/** Returns axis value for index if it is convertible to target type or throws.

  Like boost::histogram::axis::traits::value, but converts the result into the requested
  return type. If the conversion is not possible, throws std::runtime_error.

  @tparam Result requested return type
  @tparam Axis axis type
  @param axis any axis instance
  @param index floating point axis index
*/
template <class Result, class Axis>
Result value_as(const Axis& axis, real_index_type index) {
  return detail::try_cast<Result, std::runtime_error>(
      value(axis, index)); // avoid conversion warning
}

/** Returns axis index for value.

  Throws std::invalid_argument if the value argument is not implicitly convertible.

  @param axis any axis instance
  @param value argument to be passed to `index` method
*/
template <class Axis, class U,
          class _V = std::decay_t<detail::arg_type<decltype(&Axis::index)>>>
axis::index_type index(const Axis& axis,
                       const U& value) noexcept(std::is_convertible<U, _V>::value) {
  return axis.index(detail::try_cast<_V, std::invalid_argument>(value));
}

// specialization for variant
template <class... Ts, class U>
axis::index_type index(const variant<Ts...>& axis, const U& value) {
  return axis.index(value);
}

/** Returns pair of axis index and shift for the value argument.

  Throws `std::invalid_argument` if the value argument is not implicitly convertible to
  the argument expected by the `index` method. If the result of
  boost::histogram::axis::traits::static_options<decltype(axis)> has the growth flag set,
  call `update` method with the argument and return the result. Otherwise, call `index`
  and return the pair of the result and a zero shift.

  @param axis any axis instance
  @param value argument to be passed to `update` or `index` method
*/
template <class Axis, class U,
          class _V = std::decay_t<detail::arg_type<decltype(&Axis::index)>>>
std::pair<index_type, index_type> update(Axis& axis, const U& value) noexcept(
    std::is_convertible<U, _V>::value) {
  return detail::static_if_c<static_options<Axis>::test(option::growth)>(
      [&value](auto& a) {
        return a.update(detail::try_cast<_V, std::invalid_argument>(value));
      },
      [&value](auto& a) { return std::make_pair(index(a, value), index_type{0}); }, axis);
}

// specialization for variant
template <class... Ts, class U>
std::pair<index_type, index_type> update(variant<Ts...>& axis, const U& value) {
  return visit([&value](auto& a) { return a.update(value); }, axis);
}

/** Returns bin width at axis index.

  If the axis has no `value` method, throw std::runtime_error. If the method exists and
  accepts a floating point index, return the result of `axis.value(index + 1) -
  axis.value(index)`. If the method exists but accepts only integer indices, return 0.

  @param axis any axis instance
  @param index bin index
 */
template <class Axis>
decltype(auto) width(const Axis& axis, index_type index) {
  return detail::value_method_switch(
      [](const auto&) { return 0; },
      [index](const auto& a) { return a.value(index + 1) - a.value(index); }, axis);
}

/** Returns bin width at axis index.

  Like boost::histogram::axis::traits::width, but converts the result into the requested
  return type. If the conversion is not possible, throw std::runtime_error.

  @param axis any axis instance
  @param index bin index
 */
template <class Result, class Axis>
Result width_as(const Axis& axis, index_type index) {
  return detail::value_method_switch(
      [](const auto&) { return Result{}; },
      [index](const auto& a) {
        return detail::try_cast<Result, std::runtime_error>(a.value(index + 1) -
                                                            a.value(index));
      },
      axis);
}

/** Meta-function to detect whether an axis is reducible.

  Doxygen does not render this well. This is a meta-function, it accepts an axis
  type and represents std::true_type or std::false_type, depending on whether the axis can
  be reduced with boost::histogram::algorithm::reduce().

  @tparam Axis axis type.
 */
template <class Axis>
#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED
using is_reducible = std::is_constructible<Axis, const Axis&, axis::index_type,
                                           axis::index_type, unsigned>;
#else
struct is_reducible;
#endif

} // namespace traits
} // namespace axis
} // namespace histogram
} // namespace boost

#endif
