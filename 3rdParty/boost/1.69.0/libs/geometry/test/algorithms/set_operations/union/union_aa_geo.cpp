// Boost.Geometry
// Unit Test

// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

#include "test_union.hpp"

#include <boost/geometry/geometries/point_xy.hpp>


struct exterior_points_counter
{
    exterior_points_counter() : count(0) {}

    template <typename Polygon>
    void operator()(Polygon const& poly)
    {
        count += boost::size(bg::exterior_ring(poly));
    }

    std::size_t count;
};

struct interiors_counter
    : exterior_points_counter
{
    template <typename Polygon>
    void operator()(Polygon const& poly)
    {
        count += boost::size(bg::interior_rings(poly));
    }
};

void test_geographic_one(std::string const& wkt1, std::string const& wkt2,
                         std::size_t count, std::size_t exterior_points_count, std::size_t interiors_count,
                         double expected_area)
{
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > point;
    typedef bg::model::polygon<point> polygon;
    typedef bg::model::multi_polygon<polygon> multipolygon;

    bg::srs::spheroid<double> sph(6378137.0000000000, 6356752.3142451793);

    bg::strategy::intersection::geographic_segments<> is(sph);
    bg::strategy::area::geographic<> as(sph);
    
    polygon p1, p2;

    boost::geometry::read_wkt(wkt1, p1);
    boost::geometry::read_wkt(wkt2, p2);

    multipolygon result;

    enum test_mode { expect_valid, expect_empty, expect_exception };

#if defined(BOOST_GEOMETRY_UNION_THROW_INVALID_OUTPUT_EXCEPTION)
    test_mode mode = expected_area >= 0 ? expect_valid : expect_exception;
#elif defined(BOOST_GEOMETRY_UNION_RETURN_INVALID)
    test_mode mode = expect_valid;
#else
    // default
    test_mode mode = expected_area >= 0 ? expect_valid : expect_empty;
#endif

    if (mode == expect_exception)
    {
        BOOST_CHECK_THROW(boost::geometry::union_(p1, p2, result, is),
                          bg::invalid_output_exception);
    }
    else
    {

        boost::geometry::union_(p1, p2, result, is);

        double result_area = bg::area(result, as);

        std::size_t result_count = boost::size(result);
        std::size_t result_exterior_points = std::for_each(boost::begin(result),
                                                           boost::end(result),
                                                           exterior_points_counter()).count;
        std::size_t result_interiors = std::for_each(boost::begin(result),
                                                     boost::end(result),
                                                     interiors_counter()).count;
        if (mode == expect_valid)
        {
            BOOST_CHECK_EQUAL(result_count, count);
            BOOST_CHECK_EQUAL(result_exterior_points, exterior_points_count);
            BOOST_CHECK_EQUAL(result_interiors, interiors_count);
            BOOST_CHECK_CLOSE(result_area, expected_area, 0.001);
        }
        else
        {
            BOOST_CHECK_EQUAL(result_count, 0u);
            BOOST_CHECK_EQUAL(result_area, 0.0);
        }
    }
}


void test_geographic()
{
    // input ok and the result is ok
    test_geographic_one("POLYGON((16 15,-132 10,-56 89,67 5,16 15))",
                        "POLYGON((101 49,12 40,-164 10,117 0,101 49))",
                        1, 9, 0, 144265751613509.06);

    // input ok but the result is too big
    test_geographic_one("POLYGON((16 -15,-132 -22,-56 89,67 -29,16 -15))",
                        "POLYGON((101 49,12 40,-164 -21,117 -61,101 49))",
                        1, 9, 0, -163427005620080.0);

    // the second polygon is reversed i.e. it covers more than half of the globe
    // so the result is also too big
    test_geographic_one("POLYGON((16 -15,-132 -22,-56 89,67 -29,16 -15))",
                        "POLYGON((101 49,117 -61,-164 -21,12 40,101 49))",
                        1, 7, 0, -125258931656228.08);
}


int test_main(int, char* [])
{
    test_geographic();

    return 0;
}
