// Boost.Geometry
// Unit Test

// Copyright (c) 2019 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2019.
// Modifications copyright (c) 2019, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/detail/make/make.hpp>
#include <boost/geometry/geometries/infinite_line.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/util/math.hpp>

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
void test_make()
{
    typedef bg::model::infinite_line<T> line_type;

    // Horizontal through origin
    line_type line = bg::detail::make::make_infinite_line<T>(0, 0, 10, 0);
    verify_point_on_line(line, 0, 0);
    verify_point_on_line(line, 10, 0);

    // Horizontal line above origin
    line = bg::detail::make::make_infinite_line<T>(0, 5, 10, 5);
    verify_point_on_line(line, 0, 5);
    verify_point_on_line(line, 10, 5);

    // Vertical through origin
    line = bg::detail::make::make_infinite_line<T>(0, 0, 0, 10);
    verify_point_on_line(line, 0, 0);
    verify_point_on_line(line, 0, 10);

    // Vertical line left from origin
    line = bg::detail::make::make_infinite_line<T>(5, 0, 5, 10);
    verify_point_on_line(line, 5, 0);
    verify_point_on_line(line, 5, 10);

    // Diagonal through origin
    line = bg::detail::make::make_infinite_line<T>(0, 0, 8, 10);
    verify_point_on_line(line, 0, 0);
    verify_point_on_line(line, 8, 10);

    // Diagonal not through origin
    line = bg::detail::make::make_infinite_line<T>(5, 2, -8, 10);
    verify_point_on_line(line, 5, 2);
    verify_point_on_line(line, -8, 10);
}


template <typename T>
void test_all()
{
    test_make<T>();
}

int test_main(int, char* [])
{
    test_all<double>();
    test_all<long double>();
    test_all<float>();
    test_all<int>();
    return 0;
}
