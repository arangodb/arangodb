// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_DETAIL_TRY_CAST_HPP
#define BOOST_HISTOGRAM_DETAIL_TRY_CAST_HPP

#include <boost/config.hpp>
#include <boost/core/demangle.hpp>
#include <boost/histogram/detail/cat.hpp>
#include <boost/histogram/detail/type_name.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <type_traits>

namespace boost {
namespace histogram {
namespace detail {
template <class T, class E, class U>
BOOST_NORETURN T try_cast_impl(mp11::mp_int<0>, U&&) {
  BOOST_THROW_EXCEPTION(E(cat("cannot cast ", type_name<T>(), " to ", type_name<U>())));
}

template <class T, class E, class U>
T try_cast_impl(mp11::mp_int<1>, U&& u) noexcept {
  return static_cast<T>(u);
}

template <class T, class E, class U>
decltype(auto) try_cast_impl(mp11::mp_int<2>, U&& u) noexcept {
  return std::forward<U>(u);
}

// cast fails at runtime with exception E instead of compile-time, T must be a value
template <class T, class E, class U>
decltype(auto) try_cast(U&& u) noexcept(std::is_convertible<U, T>::value) {
  return try_cast_impl<T, E>(mp11::mp_int<(std::is_convertible<U, T>::value +
                                           std::is_same<T, std::decay_t<U>>::value)>{},
                             std::forward<U>(u));
}

} // namespace detail
} // namespace histogram
} // namespace boost

#endif
