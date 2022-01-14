// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2018-2019 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2019, 2020.
// Modifications copyright (c) 2019, 2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/arithmetic/infinite_line_functions.hpp>
#include <boost/geometry/geometries/infinite_line.hpp>
#include <boost/geometry/algorithms/detail/make/make.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>

namespace
{
    // Boost.Test does not support BOOST_CHECK_CLOSE for integral types
    template <typename T>
    bool is_small(T const& value)
    {
        static long double const epsilon = 1.0e-5;
        return bg::math::abs(value) < epsilon;
    }
}

template <typename T, typename C>
void verify_point_on_line(bg::model::infinite_line<T> const& line,
                          C const& x, C const& y)
{
    BOOST_CHECK_MESSAGE(is_small(line.a * x + line.b * y + line.c),
                        "Point is not located on the line");
}

template <typename T>
void test_side_value()
{
    typedef bg::model::infinite_line<T> line_type;

    // Horizontal line going right
    line_type line = bg::detail::make::make_infinite_line<T>(0, 0, 10, 0);

    // Point above (= on left side)
    T d = bg::arithmetic::side_value(line, 5, 5);
    BOOST_CHECK_MESSAGE(d > 0, "point not on left side");

    // Point below (= on right side)
    d = bg::arithmetic::side_value(line, 5, -5);
    BOOST_CHECK_MESSAGE(d < 0, "point not on right side");

    // Diagonal not through origin, from right (down) to left (up)
    line = bg::detail::make::make_infinite_line<T>(5, 2, -7, 10);
    d = bg::arithmetic::side_value(line, 5, 2);
    BOOST_CHECK_MESSAGE(is_small(d), "point not on line");
    d = bg::arithmetic::side_value(line, -7, 10);
    BOOST_CHECK_MESSAGE(is_small(d), "point not on line");

    // vector is (-12, 8), move (-3,2) on the line from (5,2)
    d = bg::arithmetic::side_value(line, 2, 4);
    BOOST_CHECK_MESSAGE(is_small(d), "point not on line");

    // Go perpendicular (2,3) from (2,4) up, so right of the line (negative)
    d = bg::arithmetic::side_value(line, 4, 7);
    BOOST_CHECK_MESSAGE(d < 0, "point not on right side");

    // Go perpendicular (2,3) from (2,4) down, so left of the line (positive)
    d = bg::arithmetic::side_value(line, 0, 1);
    BOOST_CHECK_MESSAGE(d > 0, "point not on left side");
}


template <typename T>
void test_get_intersection()
{
    typedef bg::model::infinite_line<T> line_type;

    // Diagonal lines (first is same as in distance measure,
    // second is perpendicular and used there for distance measures)
    line_type p = bg::detail::make::make_infinite_line<T>(5, 2, -7, 10);
    line_type q = bg::detail::make::make_infinite_line<T>(4, 7, 0, 1);

    typedef bg::model::point<T, 2, bg::cs::cartesian> point_type;
    point_type ip;
    BOOST_CHECK(bg::arithmetic::intersection_point(p, q, ip));

    BOOST_CHECK_MESSAGE(is_small(bg::get<0>(ip) - 2), "x-coordinate wrong");
    BOOST_CHECK_MESSAGE(is_small(bg::get<1>(ip) - 4), "y-coordinate wrong");

    verify_point_on_line(p, bg::get<0>(ip), bg::get<1>(ip));
    verify_point_on_line(q, bg::get<0>(ip), bg::get<1>(ip));
}

template <typename T>
void test_degenerate()
{
    typedef bg::model::infinite_line<T> line_type;

    line_type line = bg::detail::make::make_infinite_line<T>(0, 0, 10, 0);
    BOOST_CHECK(! bg::arithmetic::is_degenerate(line));

    line = bg::detail::make::make_infinite_line<T>(0, 0, 0, 10);
    BOOST_CHECK(! bg::arithmetic::is_degenerate(line));

    line = bg::detail::make::make_infinite_line<T>(0, 0, 10, 10);
    BOOST_CHECK(! bg::arithmetic::is_degenerate(line));

    line = bg::detail::make::make_infinite_line<T>(0, 0, 0, 0);
    BOOST_CHECK(bg::arithmetic::is_degenerate(line));
}


template <typename T>
void test_all()
{
    test_side_value<T>();
    test_get_intersection<T>();
    test_degenerate<T>();
}

int test_main(int, char* [])
{
    test_all<double>();
    test_all<long double>();
    test_all<float>();
    test_all<int>();
    return 0;
}
