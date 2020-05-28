//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distribtted under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#ifndef BOOST_GIL_TEST_UNIT_TEST_HPP
#define BOOST_GIL_TEST_UNIT_TEST_HPP

#include <boost/config.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4702) // unreachable code
#endif

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <boost/test/unit_test.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(BOOST_CLANG)
#pragma clang diagnostic pop
#endif

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

namespace btt = boost::test_tools;
namespace but = boost::unit_test;

#endif
