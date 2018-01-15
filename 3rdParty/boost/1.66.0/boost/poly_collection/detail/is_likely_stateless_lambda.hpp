/* Copyright 2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_IS_LIKELY_STATELESS_LAMBDA_HPP
#define BOOST_POLY_COLLECTION_DETAIL_IS_LIKELY_STATELESS_LAMBDA_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <type_traits>

namespace boost{

namespace poly_collection{

namespace detail{

/* Stateless lambda expressions have one (and only one) call operator and are
 * convertible to a function pointer with the same signature. Non-lambda types
 * could satisfy this too, hence the "likely" qualifier.
 */

template<typename T>
struct has_one_operator_call_helper
{
  template<typename Q> static std::true_type  test(decltype(&Q::operator())*);
  template<typename>   static std::false_type test(...);

  using type=decltype(test<T>(nullptr));
};

template<typename T>
using has_one_operator_call=typename has_one_operator_call_helper<T>::type;

template<typename T>
struct equivalent_function_pointer
{
  template<typename Q,typename R,typename... Args>
  static auto helper(R (Q::*)(Args...)const)->R(*)(Args...);
  template<typename Q,typename R,typename... Args>
  static auto helper(R (Q::*)(Args...))->R(*)(Args...);

  using type=decltype(helper(&T::operator()));
};

template<typename T,typename=void>
struct is_likely_stateless_lambda:std::false_type{};

template<typename T>
struct is_likely_stateless_lambda<
  T,
  typename std::enable_if<has_one_operator_call<T>::value>::type
>:std::is_convertible<
  T,
  typename equivalent_function_pointer<T>::type
>{};

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif
