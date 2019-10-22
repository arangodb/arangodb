// Boost.Geometry
// Unit Test

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <sstream>

#include <geometry_test_common.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <boost/geometry/algorithms/correct_closure.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>

#include <boost/variant/variant.hpp>


template <typename BaseGeometry, typename Geometry>
void check_geometry(Geometry const& geometry, std::string const& expected)
{
    std::ostringstream out;
    out << bg::wkt_manipulator<Geometry>(geometry, false);
    BOOST_CHECK_EQUAL(out.str(), expected);
}

template <typename Geometry>
void test_geometry(std::string const& wkt, std::string const& expected)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);

    // Test tye type
    bg::correct_closure(geometry);
    check_geometry<Geometry>(geometry, expected);

    // Test varianted type
    boost::variant<Geometry> v(geometry);
    bg::correct_closure(v);
    check_geometry<Geometry>(v, expected);
}

template <typename P>
void test_all()
{
    typedef bg::model::ring<P, true, true> cw_closed_ring_type;
    typedef bg::model::ring<P, true, false> cw_open_ring_type;
    typedef bg::model::ring<P, false, true> ccw_closed_ring_type;
    typedef bg::model::ring<P, false, false> ccw_open_ring_type;

    // Define clockwise and counter clockwise polygon
    std::string cw_ring       = "POLYGON((0 0,0 1,1 1,1 0,0 0))";
    std::string cw_open_ring  = "POLYGON((0 0,0 1,1 1,1 0))";

    std::string ccw_ring      = "POLYGON((0 0,1 0,1 1,0 1,0 0))";
    std::string ccw_open_ring = "POLYGON((0 0,1 0,1 1,0 1))";

    // Cases which should be closed or opened
    test_geometry<cw_closed_ring_type>(cw_open_ring, cw_ring);
    test_geometry<cw_open_ring_type>(cw_ring, cw_open_ring);
    test_geometry<ccw_closed_ring_type>(ccw_open_ring, ccw_ring);
    test_geometry<ccw_open_ring_type>(ccw_ring, ccw_open_ring);

    // Cases which are incorrect but should still be closed or opened
    test_geometry<cw_closed_ring_type>(ccw_open_ring, ccw_ring);
    test_geometry<ccw_open_ring_type>(cw_ring, cw_open_ring);

    // Cases where no action is necessary (even if order is incorrect)
    test_geometry<cw_closed_ring_type>(cw_ring, cw_ring);
    test_geometry<cw_closed_ring_type>(ccw_ring, ccw_ring);
    test_geometry<cw_open_ring_type>(cw_open_ring, cw_open_ring);
    test_geometry<cw_open_ring_type>(ccw_open_ring, ccw_open_ring);
    test_geometry<ccw_closed_ring_type>(cw_ring, cw_ring);
    test_geometry<ccw_closed_ring_type>(ccw_ring, ccw_ring);
    test_geometry<ccw_open_ring_type>(cw_open_ring, cw_open_ring);
    test_geometry<ccw_open_ring_type>(ccw_open_ring, ccw_open_ring);

    // Polygon cases
    std::string cw_polygon =
            "POLYGON((0 0,0 4,4 4,4 0,0 0),(1 1,2 1,2 2,1 2,1 1))";

    std::string cw_open_polygon =
            "POLYGON((0 0,0 4,4 4,4 0),(1 1,2 1,2 2,1 2))";

    typedef bg::model::polygon<P, true, true> cw_closed_polygon_type;
    typedef bg::model::polygon<P, true, false> cw_open_polygon_type;

    test_geometry<cw_closed_polygon_type>(cw_open_polygon, cw_polygon);
    test_geometry<cw_open_polygon_type>(cw_polygon, cw_open_polygon);

    test_geometry<cw_closed_polygon_type>(cw_polygon, cw_polygon);
    test_geometry<cw_open_polygon_type>(cw_open_polygon, cw_open_polygon);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();

    test_all<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_all<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

    return 0;
}
