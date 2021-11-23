// Boost.Geometry
// Unit Test

// Copyright (c) 2018-2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_buffer_geo.hpp"

static std::string const simplex = "POINT(4.9 52.0)";

template <bool Clockwise, typename PointType>
void test_point()
{
    typedef bg::model::polygon<PointType, Clockwise> polygon;

    bg::strategy::buffer::join_miter join_miter;
    bg::strategy::buffer::end_flat end_flat;

    // NOTE: for now do not test with a radius less than 2 meter, because is not precise yet (in double)
    test_one_geo<PointType, polygon>("simplex1", simplex, join_miter, end_flat, 70.7107, 5.0, ut_settings(0.1, false, 8));
    test_one_geo<PointType, polygon>("simplex1", simplex, join_miter, end_flat, 76.5437, 5.0, ut_settings(0.1, false, 16));
    // * Result is different for clang/VCC. Specified expectation is in between, and tolerance higher
    test_one_geo<PointType, polygon>("simplex1", simplex, join_miter, end_flat, 77.9640, 5.0, ut_settings(0.2, false, 32));

    // The more points used for the buffer, the more the area approaches 10*PI square meters
    test_one_geo<PointType, polygon>("simplex1", simplex, join_miter, end_flat, 282.8430, 10.0, ut_settings(0.1, false, 8));
    test_one_geo<PointType, polygon>("simplex1", simplex, join_miter, end_flat, 306.1471, 10.0, ut_settings(0.1, false, 16));
    test_one_geo<PointType, polygon>("simplex1", simplex, join_miter, end_flat, 312.1450, 10.0, ut_settings(0.1, false, 32));
    // * Same here
    test_one_geo<PointType, polygon>("simplex1", simplex, join_miter, end_flat, 313.9051, 10.0, ut_settings(0.2, false, 180));
}

int test_main(int, char* [])
{
    BoostGeometryWriteTestConfiguration();

    test_point<true, bg::model::point<default_test_type, 2, bg::cs::geographic<bg::degree> > >();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_point<true, bg::model::point<long double, 2, bg::cs::geographic<bg::degree> > >();
#endif

    return 0;
}
