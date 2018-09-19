// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2014 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <test_buffer.hpp>


static std::string const simplex = "POINT(5 5)";

template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::polygon<P, Clockwise> polygon;

    bg::strategy::buffer::join_miter join_miter;
    bg::strategy::buffer::end_flat end_flat;

    double const pi = boost::geometry::math::pi<double>();

    test_one<P, polygon>("simplex1", simplex, join_miter, end_flat, pi, 1.0);
    test_one<P, polygon>("simplex2", simplex, join_miter, end_flat, pi * 4.0, 2.0, ut_settings(0.1));
    test_one<P, polygon>("simplex3", simplex, join_miter, end_flat, pi * 9.0, 3.0, ut_settings(0.1));
}


int test_main(int, char* [])
{
    test_all<true, bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all<false, bg::model::point<double, 2, bg::cs::cartesian> >();
    return 0;
}
