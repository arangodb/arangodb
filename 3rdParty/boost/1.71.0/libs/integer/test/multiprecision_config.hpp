// Copyright (c) 2018 Andrey Semashev
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_INTEGER_TEST_MULTIPRECISION_CONFIG_HPP_INCLUDED_
#define BOOST_INTEGER_TEST_MULTIPRECISION_CONFIG_HPP_INCLUDED_

#include <boost/config.hpp>

#if (defined(BOOST_MSVC) && (BOOST_MSVC < 1500)) || \
    (defined(__clang_major__) && (__clang_major__ == 3) && (__clang_minor__ < 2)) || \
    (defined(BOOST_GCC) && defined(BOOST_GCC_CXX11) && BOOST_GCC < 40800)
#define DISABLE_MP_TESTS
#endif

#endif // BOOST_INTEGER_TEST_MULTIPRECISION_CONFIG_HPP_INCLUDED_
