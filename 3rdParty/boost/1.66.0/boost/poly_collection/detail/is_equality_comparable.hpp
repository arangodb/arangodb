/* Copyright 2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_IS_EQUALITY_COMPARABLE_HPP
#define BOOST_POLY_COLLECTION_DETAIL_IS_EQUALITY_COMPARABLE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp>
#include <type_traits>

#if !defined(BOOST_NO_SFINAE_EXPR)
#include <utility>
#else
#include <boost/poly_collection/detail/is_likely_stateless_lambda.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#endif

namespace boost{

namespace poly_collection{

namespace detail{

#if !defined(BOOST_NO_SFINAE_EXPR)

/* trivial, expression SFINAE-based implementation */

template<typename T,typename=void>
struct is_equality_comparable:std::false_type{};

template<typename T>
struct is_equality_comparable<
  T,
  typename std::enable_if<
    std::is_convertible<
      decltype(std::declval<T>()==std::declval<T>()),bool
    >::value
  >::type
>:std::true_type{};

#else
/* boost::has_equal_to does a decent job without using expression SFINAE,
 * but it produces a compile error when the type T being checked is
 * convertible to an equality-comparable type Q. Exotic as it may seem,
 * this is exactly the situation with the very important case of stateless
 * lambda expressions, which are convertible to an equality-comparable
 * function pointer with the same signature. We take explicit care of
 * stateless lambdas then.
 */

template<typename T,typename=void>
struct is_equality_comparable:std::integral_constant<
  bool,
  has_equal_to<T,T,bool>::value
>{};

template<typename T>
struct is_equality_comparable<
  T,
  typename std::enable_if<is_likely_stateless_lambda<T>::value>::type
>:
#if !defined(BOOST_MSVC)
  std::true_type{};
#else
  /* To complicate things further, in VS stateless lambdas are convertible not
   * only to regular function pointers, but also to other call conventions
   * such as __stdcall, __fastcall, etc., which makes equality comparison
   * ambiguous.
   */

  std::false_type{};
#endif
#endif

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif
