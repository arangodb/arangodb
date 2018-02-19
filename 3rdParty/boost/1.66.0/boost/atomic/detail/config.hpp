/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2012 Hartmut Kaiser
 * Copyright (c) 2014 Andrey Semashev
 */
/*!
 * \file   atomic/detail/config.hpp
 *
 * This header defines configuraion macros for Boost.Atomic
 */

#ifndef BOOST_ATOMIC_DETAIL_CONFIG_HPP_INCLUDED_
#define BOOST_ATOMIC_DETAIL_CONFIG_HPP_INCLUDED_

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(__has_builtin)
#if __has_builtin(__builtin_memcpy)
#define BOOST_ATOMIC_DETAIL_HAS_BUILTIN_MEMCPY
#endif
#if __has_builtin(__builtin_memcmp)
#define BOOST_ATOMIC_DETAIL_HAS_BUILTIN_MEMCMP
#endif
#elif defined(BOOST_GCC)
#define BOOST_ATOMIC_DETAIL_HAS_BUILTIN_MEMCPY
#define BOOST_ATOMIC_DETAIL_HAS_BUILTIN_MEMCMP
#endif

#if defined(BOOST_ATOMIC_DETAIL_HAS_BUILTIN_MEMCPY)
#define BOOST_ATOMIC_DETAIL_MEMCPY __builtin_memcpy
#else
#define BOOST_ATOMIC_DETAIL_MEMCPY std::memcpy
#endif

#if defined(BOOST_ATOMIC_DETAIL_HAS_BUILTIN_MEMCMP)
#define BOOST_ATOMIC_DETAIL_MEMCMP __builtin_memcmp
#else
#define BOOST_ATOMIC_DETAIL_MEMCMP std::memcmp
#endif

#if defined(__CUDACC__)
// nvcc does not support alternatives in asm statement constraints
#define BOOST_ATOMIC_DETAIL_NO_ASM_CONSTRAINT_ALTERNATIVES
// nvcc does not support condition code register ("cc") clobber in asm statements
#define BOOST_ATOMIC_DETAIL_NO_ASM_CLOBBER_CC
#endif

#if !defined(BOOST_ATOMIC_DETAIL_NO_ASM_CLOBBER_CC)
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC "cc"
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA "cc",
#else
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC
#define BOOST_ATOMIC_DETAIL_ASM_CLOBBER_CC_COMMA
#endif

#if ((defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)) && (defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) < 403)) ||\
    (defined(BOOST_GCC) && (BOOST_GCC+0) >= 70000) /* gcc 7 emits assembler warnings when zero displacement is implied */ ||\
    defined(__SUNPRO_CC)
// This macro indicates we're using older binutils that don't support implied zero displacements for memory opereands,
// making code like this invalid:
//   movl 4+(%%edx), %%eax
#define BOOST_ATOMIC_DETAIL_NO_ASM_IMPLIED_ZERO_DISPLACEMENTS
#endif

#if defined(__clang__) || (defined(BOOST_GCC) && (BOOST_GCC+0) < 40500) || defined(__SUNPRO_CC)
// This macro indicates that the compiler does not support allocating rax:rdx register pairs ("A") in asm blocks
#define BOOST_ATOMIC_DETAIL_NO_ASM_RAX_RDX_PAIRS
#endif

#if defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)
#if !(defined(BOOST_LIBSTDCXX11) && (BOOST_LIBSTDCXX_VERSION+0) >= 40700) /* libstdc++ from gcc >= 4.7 in C++11 mode */
// This macro indicates that there is no <type_traits> standard header that is sufficient for Boost.Atomic needs.
#define BOOST_ATOMIC_DETAIL_NO_CXX11_HDR_TYPE_TRAITS
#endif
#endif // defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)

// Enable pointer/reference casts between storage and value when possible.
// Note: Despite that MSVC does not employ strict aliasing rules for optimizations
// and does not require an explicit markup for types that may alias, we still don't
// enable the optimization for this compiler because at least MSVC-8 and 9 are known
// to generate broken code sometimes when casts are used.
#define BOOST_ATOMIC_DETAIL_MAY_ALIAS BOOST_MAY_ALIAS
#if !defined(BOOST_NO_MAY_ALIAS)
#define BOOST_ATOMIC_DETAIL_STORAGE_TYPE_MAY_ALIAS
#endif

#if defined(__GCC_ASM_FLAG_OUTPUTS__)
// The compiler supports output values in flag registers.
// See: https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html, Section 6.44.3.
#define BOOST_ATOMIC_DETAIL_ASM_HAS_FLAG_OUTPUTS
#endif

#if defined(__has_builtin)
#if __has_builtin(__builtin_constant_p)
#define BOOST_ATOMIC_DETAIL_IS_CONSTANT(x) __builtin_constant_p(x)
#endif
#elif defined(__GNUC__)
#define BOOST_ATOMIC_DETAIL_IS_CONSTANT(x) __builtin_constant_p(x)
#endif

#if !defined(BOOST_ATOMIC_DETAIL_IS_CONSTANT)
#define BOOST_ATOMIC_DETAIL_IS_CONSTANT(x) false
#endif

#endif // BOOST_ATOMIC_DETAIL_CONFIG_HPP_INCLUDED_
