// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_is_empty
#endif

#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include <boost/variant/variant.hpp>

#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/num_points.hpp>

#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/io/dsv/write.hpp>

namespace bg = boost::geometry;


typedef bg::model::point<double, 2, bg::cs::cartesian> point;
typedef bg::model::linestring<point> linestring;
typedef bg::model::segment<point> segment;
typedef bg::model::box<point> box;
typedef bg::model::ring<point, true, true> ring_cw_closed;
typedef bg::model::ring<point, true, false> ring_cw_open;
typedef bg::model::ring<point, false, true> ring_ccw_closed;
typedef bg::model::ring<point, false, false> ring_ccw_open;
typedef bg::model::polygon<point, true, true> polygon_cw_closed;
typedef bg::model::polygon<point, true, false> polygon_cw_open;
typedef bg::model::polygon<point, false, true> polygon_ccw_closed;
typedef bg::model::polygon<point, false, false> polygon_ccw_open;
typedef bg::model::multi_point<point> multi_point;
typedef bg::model::multi_linestring<linestring> multi_linestring;
typedef bg::model::multi_polygon<polygon_cw_closed> multi_polygon_cw_closed;
typedef bg::model::multi_polygon<polygon_cw_open> multi_polygon_cw_open;
typedef bg::model::multi_polygon<polygon_ccw_closed> multi_polygon_ccw_closed;
typedef bg::model::multi_polygon<polygon_ccw_open> multi_polygon_ccw_open;

template <std::size_t D, typename T = double>
struct box_dD
{
    typedef boost::geometry::model::box
        <
            boost::geometry::model::point<T, D, boost::geometry::cs::cartesian>
        > type;
};

template <typename Geometry, typename Tag = typename bg::tag<Geometry>::type>
struct test_is_empty
{
    static inline void apply(Geometry const& geometry, bool expected)
    {
        bool detected = bg::is_empty(geometry);
        BOOST_CHECK_MESSAGE( detected == expected,
                             std::boolalpha
                             << "Expected: " << expected
                             << " detected: " << detected
                             << " wkt: " << bg::wkt(geometry)
                             << std::noboolalpha );
        BOOST_CHECK_EQUAL(detected, bg::num_points(geometry) == 0);
    }

    static inline void apply(std::string const& wkt, bool expected)
    {
        Geometry geometry;
        bg::read_wkt(wkt, geometry);
        apply(geometry, expected);
    }
};

template <typename Box>
struct test_is_empty<Box, bg::box_tag>
{
    static inline void apply(Box const& box, bool expected)
    {
        bool detected = bg::is_empty(box);
        BOOST_CHECK_MESSAGE( detected == expected,
                             std::boolalpha
                             << "Expected: " << expected
                             << " detected: " << detected
                             << " dsv: " << bg::dsv(box)
                             << std::noboolalpha );
        BOOST_CHECK_EQUAL(detected, bg::num_points(box) == 0);
    }

    static inline void apply(std::string const& wkt, bool expected)
    {
        Box box;
        bg::read_wkt(wkt, box);
        apply(box, expected);
    }
};

BOOST_AUTO_TEST_CASE( test_point )
{
    test_is_empty<point>::apply("POINT(0 0)", false);
    test_is_empty<point>::apply("POINT(1 1)", false);
}

BOOST_AUTO_TEST_CASE( test_segment )
{
    test_is_empty<segment>::apply("SEGMENT(0 0,0 0)", false);
    test_is_empty<segment>::apply("SEGMENT(0 0,1 1)", false);
}

BOOST_AUTO_TEST_CASE( test_box )
{
    test_is_empty<box>::apply("BOX(0 0,1 1)", false);

    // test higher-dimensional boxes
    test_is_empty<box_dD<3>::type>::apply("BOX(0 0 0,1 1 1)", false);
    test_is_empty<box_dD<4>::type>::apply("BOX(0 0 0 0,1 1 1 1)", false);
    test_is_empty<box_dD<5>::type>::apply("BOX(0 0 0 0 0,1 1 1 1 1)", false);
}

BOOST_AUTO_TEST_CASE( test_linestring )
{
    typedef test_is_empty<linestring> tester;

    tester::apply("LINESTRING()", true);
    tester::apply("LINESTRING(0 0)", false);
    tester::apply("LINESTRING(0 0,0 0)", false);
    tester::apply("LINESTRING(0 0,0 0,1 1)", false);
    tester::apply("LINESTRING(0 0,0 0,0 0,1 1)", false);
}

BOOST_AUTO_TEST_CASE( test_multipoint )
{
    typedef test_is_empty<multi_point> tester;

    tester::apply("MULTIPOINT()", true);
    tester::apply("MULTIPOINT(0 0)", false);
    tester::apply("MULTIPOINT(0 0,0 0)", false);
    tester::apply("MULTIPOINT(0 0,0 0,1 1)", false);
}

BOOST_AUTO_TEST_CASE( test_multilinestring )
{
    typedef test_is_empty<multi_linestring> tester;

    tester::apply("MULTILINESTRING()", true);
    tester::apply("MULTILINESTRING(())", true);
    tester::apply("MULTILINESTRING((),())", true);
    tester::apply("MULTILINESTRING((),(0 0))", false);
    tester::apply("MULTILINESTRING((),(0 0),())", false);
    tester::apply("MULTILINESTRING((0 0))", false);
    tester::apply("MULTILINESTRING((0 0,1 0))", false);
    tester::apply("MULTILINESTRING((),(),(0 0,1 0))", false);
    tester::apply("MULTILINESTRING((0 0,1 0,0 1),(0 0,1 0,0 1,0 0))", false);
}

template <typename OpenRing>
void test_open_ring()
{
    typedef test_is_empty<OpenRing> tester;

    tester::apply("POLYGON(())", true);
    tester::apply("POLYGON((0 0))", false);
    tester::apply("POLYGON((0 0,1 0))", false);
    tester::apply("POLYGON((0 0,1 0,0 1))", false);
    tester::apply("POLYGON((0 0,0 0,1 0,0 1))", false);
}

template <typename ClosedRing>
void test_closed_ring()
{
    typedef test_is_empty<ClosedRing> tester;

    tester::apply("POLYGON(())", true);
    tester::apply("POLYGON((0 0))", false);
    tester::apply("POLYGON((0 0,0 0))", false);
    tester::apply("POLYGON((0 0,1 0,0 0))", false);
    tester::apply("POLYGON((0 0,1 0,0 1,0 0))", false);
    tester::apply("POLYGON((0 0,1 0,1 0,0 1,0 0))", false);
}

BOOST_AUTO_TEST_CASE( test_ring )
{
    test_open_ring<ring_ccw_open>();
    test_open_ring<ring_cw_open>();
    test_closed_ring<ring_ccw_closed>();
    test_closed_ring<ring_cw_closed>();
}

template <typename OpenPolygon>
void test_open_polygon()
{
    typedef test_is_empty<OpenPolygon> tester;

    tester::apply("POLYGON(())", true);
    tester::apply("POLYGON((),())", true);
    tester::apply("POLYGON((),(),())", true);
    tester::apply("POLYGON((),(0 0,0 1,1 0))", false);
    tester::apply("POLYGON((),(0 0,0 1,1 0),())", false);
    tester::apply("POLYGON((),(),(0 0,0 1,1 0))", false);
    tester::apply("POLYGON((0 0))", false);
    tester::apply("POLYGON((0 0,10 0),(0 0))", false);
    tester::apply("POLYGON((0 0,10 0),(1 1,2 1))", false);
    tester::apply("POLYGON((0 0,10 0,0 10))", false);
    tester::apply("POLYGON((0 0,10 0,0 10),())", false);
    tester::apply("POLYGON((0 0,10 0,0 10),(1 1))", false);
    tester::apply("POLYGON((0 0,10 0,0 10),(1 1,2 1))", false);
    tester::apply("POLYGON((0 0,10 0,0 10),(1 1,2 1,1 2))", false);
    tester::apply("POLYGON((0 0,10 0,10 10,0 10),(1 1,2 1,1 2))", false);
}

template <typename ClosedPolygon>
void test_closed_polygon()
{
    typedef test_is_empty<ClosedPolygon> tester;

    tester::apply("POLYGON(())", true);
    tester::apply("POLYGON((),())", true);
    tester::apply("POLYGON((),(),())", true);
    tester::apply("POLYGON((),(0 0,0 1,1 0,0 0))", false);
    tester::apply("POLYGON((),(0 0,0 1,1 0,0 0),())", false);
    tester::apply("POLYGON((),(),(0 0,0 1,1 0,0 0))", false);
    tester::apply("POLYGON((0 0))", false);
    tester::apply("POLYGON((0 0,10 0,0 0),(0 0))", false);
    tester::apply("POLYGON((0 0,10 0,0 0),(1 1,2 1,1 1))", false);
    tester::apply("POLYGON((0 0,10 0,0 10,0 0))", false);
    tester::apply("POLYGON((0 0,10 0,0 10,0 0),())", false);
    tester::apply("POLYGON((0 0,10 0,0 10,0 0),(1 1))", false);
    tester::apply("POLYGON((0 0,10 0,0 10,0 0),(1 1,2 1,1 1))", false);
    tester::apply("POLYGON((0 0,10 0,0 10,0 0),(1 1,2 1,1 2,1 1))", false);
    tester::apply("POLYGON((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,1 2,1 1))", false);
}

BOOST_AUTO_TEST_CASE( test_polygon )
{
    test_open_polygon<polygon_ccw_open>();
    test_open_polygon<polygon_cw_open>();
    test_closed_polygon<polygon_ccw_closed>();
    test_closed_polygon<polygon_cw_closed>();
}

template <typename OpenMultiPolygon>
void test_open_multipolygon()
{
    typedef test_is_empty<OpenMultiPolygon> tester;

    tester::apply("MULTIPOLYGON()", true);
    tester::apply("MULTIPOLYGON((()))", true);
    tester::apply("MULTIPOLYGON(((),()))", true);
    tester::apply("MULTIPOLYGON(((),()),((),(),()))", true);
    tester::apply("MULTIPOLYGON(((),()),((),(0 0,10 0,10 10,0 10),()))", false);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10),(1 1,2 1,1 2)))", false);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10),(1 1,2 1,2 2,1 2),(5 5,6 5,6 6,5 6)))", false);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10),(1 1,2 1,1 2)),((100 100,110 100,110 110),(101 101,102 101,102 102)))", false);
}

template <typename ClosedMultiPolygon>
void test_closed_multipolygon()
{
    typedef test_is_empty<ClosedMultiPolygon> tester;

    tester::apply("MULTIPOLYGON()", true);
    tester::apply("MULTIPOLYGON((()))", true);
    tester::apply("MULTIPOLYGON(((),()))", true);
    tester::apply("MULTIPOLYGON(((),()),((),(),()))", true);
    tester::apply("MULTIPOLYGON(((),()),((),(0 0,10 0,10 10,0 10,0 0),()))", false);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,1 2,1 1)))", false);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,2 2,1 2,1 1),(5 5,6 5,6 6,5 6,5 5)))", false);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,1 2,1 1)),((100 100,110 100,110 110,100 100),(101 101,102 101,102 102,101 101)))", false);
}

BOOST_AUTO_TEST_CASE( test_multipolygon )
{
    test_open_multipolygon<multi_polygon_ccw_open>();
    test_open_multipolygon<multi_polygon_cw_open>();
    test_closed_multipolygon<multi_polygon_ccw_closed>();
    test_closed_multipolygon<multi_polygon_cw_closed>();
}

BOOST_AUTO_TEST_CASE( test_variant )
{
    typedef boost::variant
        <
            linestring, polygon_cw_open, polygon_cw_closed, multi_point
        > variant_geometry_type;

    typedef test_is_empty<variant_geometry_type> tester;

    linestring ls_empty;
    bg::read_wkt("LINESTRING()", ls_empty);

    linestring ls;
    bg::read_wkt("LINESTRING(1 1,2 2,5 6)", ls);

    polygon_cw_open p_open;
    bg::read_wkt("POLYGON((0 0,0 1,1 0))", p_open);

    polygon_cw_closed p_closed;
    bg::read_wkt("POLYGON(())", p_closed);

    multi_point mp;
    bg::read_wkt("MULTIPOINT((1 10))", mp);

    multi_point mp_empty;
    bg::read_wkt("MULTIPOINT((1 10))", mp);

    variant_geometry_type variant_geometry;

    variant_geometry = ls_empty;
    tester::apply(variant_geometry, true);

    variant_geometry = p_open;
    tester::apply(variant_geometry, false);

    variant_geometry = p_closed;
    tester::apply(variant_geometry, true);

    variant_geometry = mp;
    tester::apply(variant_geometry, false);

    variant_geometry = mp_empty;
    tester::apply(variant_geometry, true);

    variant_geometry = ls;
    tester::apply(variant_geometry, false);
}
