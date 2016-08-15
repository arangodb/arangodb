// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_num_interior_rings
#endif

#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include <boost/variant/variant.hpp>

#include <boost/geometry/algorithms/num_interior_rings.hpp>

#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/io/dsv/write.hpp>

namespace bg = boost::geometry;


typedef bg::model::point<double, 2, bg::cs::cartesian> point;
typedef bg::model::linestring<point> linestring;
typedef bg::model::segment<point> segment;
typedef bg::model::box<point> box;
typedef bg::model::ring<point> ring;
typedef bg::model::polygon<point> polygon;
typedef bg::model::multi_point<point> multi_point;
typedef bg::model::multi_linestring<linestring> multi_linestring;
typedef bg::model::multi_polygon<polygon> multi_polygon;


template <typename Geometry>
struct test_num_interior_rings
{
    static inline void apply(Geometry const& geometry,
                             std::size_t expected)
    {
        std::size_t detected = bg::num_interior_rings(geometry);
        BOOST_CHECK_MESSAGE( detected == expected,
                             "Expected: " << expected
                             << " detected: " << detected
                             << " wkt: " << bg::wkt(geometry) );
    }

    static inline void apply(std::string const& wkt,
                             std::size_t expected)
    {
        Geometry geometry;
        bg::read_wkt(wkt, geometry);
        apply(geometry, expected);
    }
};

BOOST_AUTO_TEST_CASE( test_point )
{
    test_num_interior_rings<point>::apply("POINT(0 0)", 0);
}

BOOST_AUTO_TEST_CASE( test_segment )
{
    test_num_interior_rings<segment>::apply("SEGMENT(0 0,1 1)", 0);
}

BOOST_AUTO_TEST_CASE( test_box )
{
    test_num_interior_rings<box>::apply("BOX(0 0,1 1)", 0);
}

BOOST_AUTO_TEST_CASE( test_linestring )
{
    test_num_interior_rings<linestring>::apply("LINESTRING(0 0,1 1)", 0);
}

BOOST_AUTO_TEST_CASE( test_multipoint )
{
    test_num_interior_rings<multi_point>::apply("MULTIPOINT(0 0,1 1)", 0);
}

BOOST_AUTO_TEST_CASE( test_multilinestring )
{
    test_num_interior_rings
        <
            multi_linestring
        >::apply("MULTILINESTRING((0 0,1 0,0 1),(0 0,1 0,0 1,0 0))", 0);
}

BOOST_AUTO_TEST_CASE( test_ring )
{
    typedef test_num_interior_rings<ring> tester;

    tester::apply("POLYGON((0 0,1 0,0 1,0 0))", 0);
    tester::apply("POLYGON((0 0,1 0,1 0,0 1,0 0))", 0);
}

BOOST_AUTO_TEST_CASE( test_polygon )
{
    typedef test_num_interior_rings<polygon> tester;

    tester::apply("POLYGON((0 0,10 0,0 10,0 0))", 0);
    tester::apply("POLYGON((0 0,10 0,0 10,0 0),(1 1,2 1,1 2,1 1))", 1);
    tester::apply("POLYGON((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,2 2,1 2,1 1),(5 5,6 5,6 6,5 6,5 5))", 2);
}

BOOST_AUTO_TEST_CASE( test_multipolygon )
{
    typedef test_num_interior_rings<multi_polygon> tester;

    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0)))", 0);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,1 2,1 1)))", 1);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,2 2,1 2,1 1),(5 5,6 5,6 6,5 6,5 5)))", 2);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,1 2,1 1)),((100 100,110 100,110 110,100 100),(101 101,102 101,102 102,101 101)))", 2);
    tester::apply("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(1 1,2 1,2 2,1 2,1 1),(5 5,6 5,6 6,5 6,5 5)),((100 100,110 100,110 110,100 100),(101 101,102 101,102 102,101 101),(105 105,106 105,106 106,105 106,105 105)))", 4);
}

BOOST_AUTO_TEST_CASE( test_variant )
{
    typedef boost::variant
        <
            linestring, polygon, multi_polygon
        > variant_geometry_type;

    typedef test_num_interior_rings<variant_geometry_type> tester;

    linestring ls;
    bg::read_wkt("LINESTRING(0 0,1 1,2 2)", ls);

    polygon poly;
    bg::read_wkt("POLYGON((0 0,0 10,10 10,10 0,0 0),(1 1,9 1,9 9,1 9,1 1))", poly);

    multi_polygon mpoly;
    bg::read_wkt("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(1 1,2 1,1 2,1 1),(5 5,6 5,6 6,5 6,5 5)))", mpoly);

    variant_geometry_type variant_geometry;

    variant_geometry = ls;
    tester::apply(variant_geometry, 0);

    variant_geometry = poly;
    tester::apply(variant_geometry, 1);

    variant_geometry = mpoly;
    tester::apply(variant_geometry, 2);
}
