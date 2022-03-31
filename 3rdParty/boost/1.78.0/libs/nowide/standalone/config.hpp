//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//  Copyright (c) 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef NOWIDE_CONFIG_HPP_INCLUDED
#define NOWIDE_CONFIG_HPP_INCLUDED

#include <boost/nowide/replacement.hpp>

#if(defined(__WIN32) || defined(_WIN32) || defined(WIN32)) && !defined(__CYGWIN__)
#define NOWIDE_WINDOWS
#endif

#ifdef _MSC_VER
#define NOWIDE_MSVC _MSC_VER
#endif

#ifdef __GNUC__
#define BOOST_SYMBOL_VISIBLE __attribute__((__visibility__("default")))
#endif

#ifndef BOOST_SYMBOL_VISIBLE
#define BOOST_SYMBOL_VISIBLE
#endif

#ifdef NOWIDE_WINDOWS
#define BOOST_SYMBOL_EXPORT __declspec(dllexport)
#define BOOST_SYMBOL_IMPORT __declspec(dllimport)
#else
#define BOOST_SYMBOL_EXPORT BOOST_SYMBOL_VISIBLE
#define BOOST_SYMBOL_IMPORT
#endif

#if defined(BOOST_NOWIDE_DYN_LINK)
#ifdef BOOST_NOWIDE_SOURCE
#define BOOST_NOWIDE_DECL BOOST_SYMBOL_EXPORT
#else
#define BOOST_NOWIDE_DECL BOOST_SYMBOL_IMPORT
#endif // BOOST_NOWIDE_SOURCE
#else
#define BOOST_NOWIDE_DECL
#endif // BOOST_NOWIDE_DYN_LINK

#ifndef NOWIDE_DECL
#define NOWIDE_DECL
#endif

#if defined(NOWIDE_WINDOWS)
#ifdef BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
#undef BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT
#endif
#define BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT 1
#elif !defined(BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT)
#define BOOST_NOWIDE_USE_FILEBUF_REPLACEMENT 0
#endif

#if defined(__GNUC__) && __GNUC__ >= 7
#define BOOST_NOWIDE_FALLTHROUGH __attribute__((fallthrough))
#else
#define BOOST_NOWIDE_FALLTHROUGH
#endif

#if defined __GNUC__
#define BOOST_LIKELY(x) __builtin_expect(x, 1)
#define BOOST_UNLIKELY(x) __builtin_expect(x, 0)
#else
#if !defined(BOOST_LIKELY)
#define BOOST_LIKELY(x) x
#endif
#if !defined(BOOST_UNLIKELY)
#define BOOST_UNLIKELY(x) x
#endif
#endif

// The std::codecvt<char16/32_t, char, std::mbstate_t> are deprecated in C++20
// These macros can suppress this warning
#if defined(_MSC_VER)
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4996))
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END __pragma(warning(pop))
#elif(__cplusplus >= 202002L) && defined(__clang__)
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END _Pragma("clang diagnostic pop")
#elif(__cplusplus >= 202002L) && defined(__GNUC__)
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END _Pragma("GCC diagnostic pop")
#else
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_BEGIN
#define BOOST_NOWIDE_SUPPRESS_UTF_CODECVT_DEPRECATION_END
#endif

#endif
