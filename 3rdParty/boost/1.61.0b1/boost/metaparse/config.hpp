#ifndef BOOST_METAPARSE_CONFIG_HPP
#define BOOST_METAPARSE_CONFIG_HPP

// Copyright Abel Sinkovics (abel@sinkovics.hu)  2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

/*
 * C++11 features
 */

#if \
  !defined BOOST_NO_CXX11_VARIADIC_TEMPLATES \
  && !defined BOOST_NO_VARIADIC_TEMPLATES \
  \
  && !defined BOOST_NO_VARIADIC_TEMPLATES \
  && !defined BOOST_USE_VARIADIC_TEMPLATES

  #define BOOST_USE_VARIADIC_TEMPLATES

#endif

#if \
  !defined BOOST_NO_CONSTEXPR \
  && !defined BOOST_NO_CXX11_CONSTEXPR \
  \
  && !defined BOOST_NO_CONSTEXPR \
  && !defined BOOST_USE_CONSTEXPR

  #define BOOST_USE_CONSTEXPR

#endif

#ifdef BOOST_USE_CONSTEXPR
#  define BOOST_CONSTEXPR constexpr
#else
#  define BOOST_CONSTEXPR
#endif

/*
 * Compiler workarounds
 */

#if defined __GNUC__ && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7))
#  define BOOST_NO_CONSTEXPR_C_STR
#endif

/*
 * Metaparse config
 */

#if \
  defined BOOST_USE_VARIADIC_TEMPLATES \
  && !defined BOOST_METAPARSE_VARIADIC_STRING
#  define BOOST_METAPARSE_VARIADIC_STRING
#endif

#endif

