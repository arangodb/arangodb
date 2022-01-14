// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_is_simple
#endif

#include "test_is_simple.hpp"


namespace bg = ::boost::geometry;

typedef bg::model::point<double, 2, bg::cs::cartesian>  point_type;
typedef bg::model::segment<point_type>                  segment_type;
typedef bg::model::linestring<point_type>               linestring_type;
typedef bg::model::multi_linestring<linestring_type>    multi_linestring_type;
// ccw open and closed polygons
typedef bg::model::polygon<point_type,false,false>      open_ccw_polygon_type;
typedef bg::model::polygon<point_type,false,true>       closed_ccw_polygon_type;
// multi-geometries
typedef bg::model::multi_point<point_type>              multi_point_type;
typedef bg::model::multi_polygon<open_ccw_polygon_type> multi_polygon_type;
// box
typedef bg::model::box<point_type>                      box_type;


BOOST_AUTO_TEST_CASE( test_is_simple_point )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_simple: POINT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef point_type G;

    test_simple(from_wkt<G>("POINT(0 0)"), true);
}

BOOST_AUTO_TEST_CASE( test_is_simple_multipoint )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_simple: MULTIPOINT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef multi_point_type G;

    test_simple(from_wkt<G>("MULTIPOINT(0 0)"), true);
    test_simple(from_wkt<G>("MULTIPOINT(0 0,1 0,1 1,0 1)"), true);
    test_simple(from_wkt<G>("MULTIPOINT(0 0,1 0,1 1,1 0,0 1)"), false);

    // empty multipoint
    test_simple(from_wkt<G>("MULTIPOINT()"), true);
}

BOOST_AUTO_TEST_CASE( test_is_simple_segment )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_simple: SEGMENT " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef segment_type G;

    test_simple(from_wkt<G>("SEGMENT(0 0,1 0)"), true);
}

BOOST_AUTO_TEST_CASE( test_is_simple_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_simple: LINESTRING " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef linestring_type G;

    // valid linestrings with multiple points
    test_simple(from_wkt<G>("LINESTRING(0 0,0 0,1 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,0 0,1 0,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,0 0,1 0,1 0,1 1,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,2 0,1 1,1 0,1 -1)"), false);

    // simple open linestrings
    test_simple(from_wkt<G>("LINESTRING(0 0,1 2)"), true);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 2,2 3)"), true);

    // simple closed linestrings
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,1 1,0 0)"), true);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,1 1,0 1,0 0)"), true);
    test_simple(from_wkt<G>("LINESTRING(0 0,10 0,10 10,0 10,0 0)"), true);

    // non-simple linestrings
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,2 10,0.5 -1)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,2 1,1 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,2 1,0.5 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,2 0,1 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,3 0,5 0,1 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,3 0,5 0,4 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,3 0,5 0,4 0,2 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,3 0,2 0,5 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,2 0,2 2,1 0,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,2 0,2 2,1 0,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,10 0,10 10,0 10,0 0,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,0 10,5 10,0 0,10 10,10 5,10 0,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,0 0,10 0,10 10,0 10,0 0,0 0)"),
                false);
    test_simple(from_wkt<G>("LINESTRING(0 0,0 0,0 0,10 0,10 10,0 10,0 0,0 0,0 0,0 0)"),
                false);
    test_simple(from_wkt<G>("LINESTRING(0 0,0 0,10 0,10 10,10 10,10 10,10 10,10 10,0 10,0 0,0 0)"),
                false);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,2 0,2 2,1 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(1 0,2 2,2 0,1 0,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(0 0,1 0,2 0,2 2,1 0,1 4,0 0)"), false);
    test_simple(from_wkt<G>("LINESTRING(4 1,10 8,4 6,4 1,10 5,10 3)"),
                false);
    test_simple(from_wkt<G>("LINESTRING(10 3,10 5,4 1,4 6,10 8,4 1)"),
                false);

    // empty linestring
    // the simplicity result is irrelevant since an empty linestring
    // is considered as invalid
    test_simple(from_wkt<G>("LINESTRING()"), false, false);
}

BOOST_AUTO_TEST_CASE( test_is_simple_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_simple: MULTILINESTRING " << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef multi_linestring_type G;

    // multilinestrings with linestrings with spikes
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,0 0),(5 0,6 0,7 0))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,0 0),(5 0,1 0,4 1))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,0 0),(5 0,1 0,4 0))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,0 0),(1 0,2 0))"),
                false);

    // simple multilinestrings
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 1),(1 1,1 0))"), true);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 1),(1 1,1 0),(0 1,1 1))"),
                true);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2),(0 0,1 0,2 0,2 2))"), true);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2),(2 2,2 0,1 0,0 0))"), true);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0),(0 0,-1 0),(1 0,2 0))"),
                true);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0),(-1 0,0 0),(2 0,1 0))"),
                true);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0),(0 0,0 1),(0 0,-1 0),(0 0,0 -1))"),
                true);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,10 0,10 10,0 10,0 0))"), true);

    // non-simple multilinestrings
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2),(0 0,2 2))"), false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2),(2 2,0 0))"), false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2),(0 0,1 0,1 1,2 0,2 2))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 1,2 2),(0 0,1 0,1 1,2 0,2 2))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 1,2 2),(2 2,0 0))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2,4 4),(0 0,1 1))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2,4 4),(0 0,3 3))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2,4 4),(1 1,3 3))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2,4 4),(1 1,2 2))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2,4 4),(2 2,3 3))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2,4 4),(2 2,4 4))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 2,4 4),(4 4,2 2))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 1),(0 1,1 0))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,2 0),(1 0,0 1))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 1),(1 1,1 0),(1 1,0 1,0.5,0.5))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,1 1,0 1,0 0),(1 0,1 -1))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,1 1,0 1,0 0),(-1 0,0 0))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,1 1,0 1,0 0),(0 0,-1 0,-1 -1,0 -1,0 0))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,1 0,1 1,0 1,0 0),(-1 -1,-1 0,0 0,0 -1,-1 -1))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((0 0,0 10,5 10,0 0,10 10,10 5,10 0,0 0))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((4 1,10 8,4 6,4 1,10 5,10 3))"),
                false);
    test_simple(from_wkt<G>("MULTILINESTRING((10 3,10 5,4 1,4 6,10 8,4 1))"),
                false);

    // empty multilinestring
    test_simple(from_wkt<G>("MULTILINESTRING()"), true);
}

BOOST_AUTO_TEST_CASE( test_is_simple_areal )
{
    typedef box_type b;
    typedef open_ccw_polygon_type o_ccw_p;
    typedef multi_polygon_type mpl;

    // check that is_simple compiles for boxes
    test_simple(from_wkt<b>("BOX(0 0,1 1)"), true);

    // simple polygons and multi-polygons
    test_simple(from_wkt<o_ccw_p>("POLYGON((0 0,1 0,1 1))"), true);
    test_simple(from_wkt<o_ccw_p>("POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 1))"),
                true);
    test_simple(from_wkt<mpl>("MULTIPOLYGON(((0 0,1 0,1 1)),((10 0,20 0,20 10,10 10)))"),
                true);

    // non-simple polygons & multi-polygons (have duplicate points)
    test_simple(from_wkt<o_ccw_p>("POLYGON((0 0,1 0,1 0,1 1))"), false);
    test_simple(from_wkt<o_ccw_p>("POLYGON((0 0,10 0,10 10,0 10),(1 1,1 9,9 9,9 9,9 1))"),
                false);
    test_simple(from_wkt<mpl>("MULTIPOLYGON(((0 0,1 0,1 1,1 1)),((10 0,20 0,20 0,20 10,10 10)))"),
                false);

    // empty polygon
    // the simplicity result is irrelevant since an empty polygon
    // is considered as invalid
    test_simple(from_wkt<o_ccw_p>("POLYGON(())"), false, false);

    // empty multipolygon
    test_simple(from_wkt<mpl>("MULTIPOLYGON()"), true);
}

BOOST_AUTO_TEST_CASE( test_geometry_with_NaN_coordinates )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_valid: geometry with NaN coordinates" << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    linestring_type ls1, ls2;
    bg::read_wkt("LINESTRING(1 1,1.115235e+308 1.738137e+308)", ls1);
    bg::read_wkt("LINESTRING(-1 1,1.115235e+308 1.738137e+308)", ls2);

    // the intersection of the two linestrings is a new linestring
    // (multilinestring with a single element) that has NaN coordinates
    multi_linestring_type mls;
    bg::intersection(ls1, ls2, mls);

    test_simple(mls, true, false);
}

BOOST_AUTO_TEST_CASE( test_is_simple_variant )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl;
    std::cout << "************************************" << std::endl;
    std::cout << " is_simple: variant support" << std::endl;
    std::cout << "************************************" << std::endl;
#endif

    typedef bg::model::polygon<point_type> polygon_type; // cw, closed
    typedef boost::variant
        <
            linestring_type, multi_linestring_type, polygon_type
        > variant_geometry;

    variant_geometry vg;

    linestring_type simple_linestring =
        from_wkt<linestring_type>("LINESTRING(0 0,1 0)");
    multi_linestring_type non_simple_multi_linestring = from_wkt
        <
            multi_linestring_type
        >("MULTILINESTRING((0 0,1 0,1 1,0 0),(10 0,1 1))");
    polygon_type simple_polygon =
        from_wkt<polygon_type>("POLYGON((0 0,1 1,1 0,0 0))");
    polygon_type non_simple_polygon =
        from_wkt<polygon_type>("POLYGON((0 0,1 1,1 0,1 0,0 0))");

    vg = simple_linestring;
    test_simple(vg, true);
    vg = non_simple_multi_linestring;
    test_simple(vg, false);
    vg = simple_polygon;
    test_simple(vg, true);
    vg = non_simple_polygon;
    test_simple(vg, false);
}
