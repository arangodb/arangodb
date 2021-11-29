// Copyright (c) 2018 Andrey Semashev
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_INTEGER_TEST_MULTIPRECISION_CONFIG_HPP_INCLUDED_
#define BOOST_INTEGER_TEST_MULTIPRECISION_CONFIG_HPP_INCLUDED_

#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#if (defined(BOOST_MSVC) && (BOOST_MSVC < 1500)) || \
    (defined(__clang_major__) && (__clang_major__ == 3) && (__clang_minor__ < 2)) || \
    (defined(BOOST_GCC) && defined(BOOST_GCC_CXX11) && BOOST_GCC < 40800)
#define DISABLE_MP_TESTS
#endif

// This list of checks matches those in Boost.Multiprecision, boost/multiprecision/detail/number_base.hpp,
// as it no longer supports C++03 since 2021.
#if !defined(DISABLE_MP_TESTS) && \
    (\
        defined(BOOST_NO_CXX11_RVALUE_REFERENCES) || defined(BOOST_NO_CXX11_TEMPLATE_ALIASES) || defined(BOOST_NO_CXX11_HDR_ARRAY) || defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)\
        || defined(BOOST_NO_CXX11_ALLOCATOR) || defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) || defined(BOOST_NO_CXX11_CONSTEXPR)\
        || (defined(BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS) || BOOST_WORKAROUND(__SUNPRO_CC, < 0x5140)) || defined(BOOST_NO_CXX11_REF_QUALIFIERS) || defined(BOOST_NO_CXX11_HDR_FUNCTIONAL)\
        || defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) || defined(BOOST_NO_CXX11_USER_DEFINED_LITERALS) || defined(BOOST_NO_CXX11_THREAD_LOCAL)\
        || defined(BOOST_NO_CXX11_DECLTYPE) || defined(BOOST_NO_CXX11_STATIC_ASSERT) || defined(BOOST_NO_CXX11_DEFAULTED_FUNCTIONS)\
        || defined(BOOST_NO_CXX11_NOEXCEPT) || defined(BOOST_NO_CXX11_REF_QUALIFIERS)\
    )
#define DISABLE_MP_TESTS
#endif

#endif // BOOST_INTEGER_TEST_MULTIPRECISION_CONFIG_HPP_INCLUDED_
