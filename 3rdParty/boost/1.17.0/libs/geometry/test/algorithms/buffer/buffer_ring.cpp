// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017-2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_buffer.hpp"

// Short test verifying behavior of explicit ring_type (polygon without holes)
// The test buffer_polygon.cpp contains all other tests, also for rings.

static std::string const concave_simplex = "POLYGON ((0 0,3 5,3 3,5 3,0 0))";

template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::ring<P, Clockwise, true> ring_type;
    typedef bg::model::polygon<P, Clockwise, true> polygon_type;

    bg::strategy::buffer::join_miter join_miter(10.0);
    bg::strategy::buffer::join_round join_round(100);
    bg::strategy::buffer::end_flat end_flat;

    test_one<ring_type, polygon_type>("concave_simplex", concave_simplex, join_round, end_flat, 14.5616, 0.5);
    test_one<ring_type, polygon_type>("concave_simplex", concave_simplex, join_miter, end_flat, 16.3861, 0.5);

    test_one<ring_type, polygon_type>("concave_simplex", concave_simplex, join_round, end_flat, 0.777987, -0.5);
    test_one<ring_type, polygon_type>("concave_simplex", concave_simplex, join_miter, end_flat, 0.724208, -0.5);
}


int test_main(int, char* [])
{
    test_all<true, bg::model::point<double, 2, bg::cs::cartesian> >();
#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<false, bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all<true, bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all<false, bg::model::point<float, 2, bg::cs::cartesian> >();
#endif
    return 0;
}
