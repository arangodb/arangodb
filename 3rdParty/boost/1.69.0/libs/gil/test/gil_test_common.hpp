//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distribtted under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_TEST_TOOLS_HPP
#define BOOST_GIL_TEST_TEST_TOOLS_HPP

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4702) // unreachable code
#elif defined(__clang__) && defined(__has_warning)
#pragma clang diagnostic push
#elif defined(__GNUC__) && \
    !(defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <boost/test/included/unit_test.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__) && defined(__has_warning)
#pragma clang diagnostic pop
#elif defined(__GNUC__) && \
    !(defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC))
#pragma GCC diagnostic pop
#endif

namespace btt = boost::test_tools;
namespace but = boost::unit_test;

#endif
