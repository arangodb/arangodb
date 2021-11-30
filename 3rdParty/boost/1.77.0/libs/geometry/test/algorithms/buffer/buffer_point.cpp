// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_buffer.hpp"


static std::string const simplex = "POINT(5 5)";

template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::polygon<P, Clockwise> polygon;

    bg::strategy::buffer::join_miter join_miter;
    bg::strategy::buffer::end_flat end_flat;

    // The expectation is smaller than pi, because it doesn't use an unlimited number of points.
    double const pi = boost::geometry::math::pi<double>();
    double const expectation = pi *  0.99915;

    test_one<P, polygon>("simplex1", simplex, join_miter, end_flat, expectation, 1.0);
    test_one<P, polygon>("simplex2", simplex, join_miter, end_flat, expectation * 4.0, 2.0);
    test_one<P, polygon>("simplex3", simplex, join_miter, end_flat, expectation * 9.0, 3.0);
}


int test_main(int, char* [])
{
    BoostGeometryWriteTestConfiguration();

    test_all<true, bg::model::point<default_test_type, 2, bg::cs::cartesian> >();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_ORDER)
    test_all<false, bg::model::point<default_test_type, 2, bg::cs::cartesian> >();
#endif

#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    BoostGeometryWriteExpectedFailures(BG_NO_FAILURES);
#endif

    return 0;
}
