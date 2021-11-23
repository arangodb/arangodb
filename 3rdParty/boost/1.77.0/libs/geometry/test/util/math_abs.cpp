// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_math_abs
#endif

#include <cmath>
#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include <boost/config.hpp>

#include "number_types.hpp"

// important: the include above must precede the include below,
// otherwise the test will fail for the custom number type:
// custom_with_global_sqrt

#include <boost/geometry/util/math.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>

namespace bg = boost::geometry;
namespace bgm = boost::geometry::math;

template <typename T>
bool eq(T const& l, T const& r)
{
    return !(l < r || r < l);
}

BOOST_AUTO_TEST_CASE( test_math_abs )
{
    {
        float p1 = bgm::pi<float>();
        double p2 = bgm::pi<double>();
        long double p3 = bgm::pi<long double>();

        BOOST_CHECK(bgm::abs(p1) == p1);
        BOOST_CHECK(bgm::abs(p2) == p2);
        BOOST_CHECK(bgm::abs(p3) == p3);

        float n1 = -p1;
        double n2 = -p2;
        long double n3 = -p3;

        BOOST_CHECK(bgm::abs(n1) == p1);
        BOOST_CHECK(bgm::abs(n2) == p2);
        BOOST_CHECK(bgm::abs(n3) == p3);
    }

    {
        number_types::custom<double> p1(bgm::pi<double>());
        number_types::custom_with_global_sqrt<double> p2(bgm::pi<double>());
        custom_global<double> p3(bgm::pi<double>());
        custom_raw<double> p4(bgm::pi<double>());

        BOOST_CHECK(eq(bgm::abs(p1), p1));
        BOOST_CHECK(eq(bgm::abs(p2), p2));
        BOOST_CHECK(eq(bgm::abs(p3), p3));
        BOOST_CHECK(eq(bgm::abs(p4), p4));

        number_types::custom<double> n1 = -p1;
        number_types::custom_with_global_sqrt<double> n2 = -p2;
        custom_global<double> n3 = -p3;
        custom_raw<double> n4 = -p4;

        BOOST_CHECK(eq(bgm::abs(n1), p1));
        BOOST_CHECK(eq(bgm::abs(n2), p2));
        BOOST_CHECK(eq(bgm::abs(n3), p3));
        BOOST_CHECK(eq(bgm::abs(n4), p4));
    }
}

