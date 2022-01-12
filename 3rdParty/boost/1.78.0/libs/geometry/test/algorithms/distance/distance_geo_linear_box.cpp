// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017-2021 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#include <iostream>
#endif

#include <string>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_geographic_linear_areal
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_geo_common.hpp"
//#include "test_empty_geometry.hpp"


template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_segment_polygon(Strategy_pp const& strategy_pp,
                                   Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/polygon distance tests" << std::endl;
#endif
    typedef bg::model::segment<Point> segment_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef test_distance_of_geometries<segment_type, polygon_type> tester;

    std::string const polygon = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("s-r-1", "SEGMENT(0 0, 0 10)", polygon,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_linestring_polygon(Strategy_pp const& strategy_pp,
                                      Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/polygon distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef test_distance_of_geometries<linestring_type, polygon_type> tester;

    std::string const polygon = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("l-r-1", "LINESTRING(0 0, 0 10)", polygon,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_multi_linestring_polygon(Strategy_pp const& strategy_pp,
                                            Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/polygon distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef test_distance_of_geometries<multi_linestring_type, polygon_type> tester;

    std::string const polygon = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("ml-r-1", "MULTILINESTRING((0 0, 0 10)(0 0, 10 0))", polygon,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

//=====================================================================

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_segment_multi_polygon(Strategy_pp const& strategy_pp,
                                         Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/multi_polygon distance tests" << std::endl;
#endif
    typedef bg::model::segment<Point> segment_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;
    typedef test_distance_of_geometries<segment_type, multi_polygon_type> tester;

    std::string const mp = "MULTIPOLYGON(((20 20, 20 30, 30 40, 20 20)),\
                                  ((10 10,0 20, 15 30, 20 15, 15 10, 10 10)))";

    tester::apply("s-r-1", "SEGMENT(0 0, 0 10)", mp,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_linestring_multi_polygon(Strategy_pp const& strategy_pp,
                                            Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/multi_polygon distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;
    typedef test_distance_of_geometries<linestring_type, multi_polygon_type> tester;

    std::string const mp = "MULTIPOLYGON(((20 20, 20 30, 30 40, 20 20)),\
                                    ((10 10,0 20, 15 30, 20 15, 15 10, 10 10)))";

    tester::apply("l-r-1", "LINESTRING(0 0, 0 10)", mp,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_multi_linestring_multi_polygon(Strategy_pp const& strategy_pp,
                                                  Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/multi_polygon distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;
    typedef test_distance_of_geometries<multi_linestring_type, multi_polygon_type> tester;

    std::string const polygon = "MULTIPOLYGON(((20 20, 20 30, 30 40, 20 20)),\
                                   ((10 10,0 20, 15 30, 20 15, 15 10, 10 10)))";

    tester::apply("ml-r-1", "MULTILINESTRING((0 0, 0 10)(0 0, 10 0))", polygon,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

//=====================================================================

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_segment_ring(Strategy_pp const& strategy_pp,
                                Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/ring distance tests" << std::endl;
#endif
    typedef bg::model::segment<Point> segment_type;
    typedef bg::model::ring<Point> ring_type;
    typedef test_distance_of_geometries<segment_type, ring_type> tester;

    std::string const ring = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("s-r-1", "SEGMENT(0 0, 0 10)", ring,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_linestring_ring(Strategy_pp const& strategy_pp,
                                   Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/ring distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::ring<Point> ring_type;
    typedef test_distance_of_geometries<linestring_type, ring_type> tester;

    std::string const ring = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("l-r-1", "LINESTRING(0 0, 0 10)", ring,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_multi_linestring_ring(Strategy_pp const& strategy_pp,
                                         Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/ring distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;
    typedef bg::model::ring<Point> ring_type;
    typedef test_distance_of_geometries<multi_linestring_type, ring_type> tester;

    std::string const ring = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("ml-r-1", "MULTILINESTRING((0 0, 0 10)(0 0, 10 0))", ring,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, false, false);
}

//======================================================================

template <typename Point, typename Strategy_pp, typename Strategy_ps, typename Strategy_sb>
void test_distance_segment_box(Strategy_pp const& strategy_pp,
                               Strategy_ps const& strategy_ps,
                               Strategy_sb const& strategy_sb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/box distance tests" << std::endl;
#endif
    typedef bg::model::segment<Point> segment_type;
    typedef bg::model::box<Point> box_type;
    typedef test_distance_of_geometries<segment_type, box_type> tester;

    std::string const box_north = "BOX(10 10,20 20)";

    tester::apply("sb1-1a", "SEGMENT(0 0, 0 20)", box_north,
                  pp_distance<Point>("POINT(0 20)", "POINT(10 20)", strategy_pp),
                  strategy_sb);
    //segment with slope
    tester::apply("sb1-1b", "SEGMENT(10 5, 20 6)", box_north,
                  pp_distance<Point>("POINT(20 6)", "POINT(20 10)", strategy_pp),
                  strategy_sb);
    tester::apply("sb1-2", "SEGMENT(0 0, 0 10)", box_north,
                  ps_distance<Point>("POINT(0 10)", "SEGMENT(10 10,10 20)", strategy_ps),
                  strategy_sb);
    tester::apply("sb1-3", "SEGMENT(0 0, 0 15)", box_north,
                  ps_distance<Point>("POINT(0 15)", "SEGMENT(10 10,10 20)", strategy_ps),
                  strategy_sb);
    tester::apply("sb1-4", "SEGMENT(0 0, 0 25)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(0 0,0 25)", strategy_ps),
                  strategy_sb);
    tester::apply("sb1-5", "SEGMENT(0 10, 0 25)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(0 0,0 25)", strategy_ps),
                  strategy_sb);

    tester::apply("sb2-2", "SEGMENT(0 5, 15 5)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(0 5,15 5)", strategy_ps),
                  strategy_sb);
    tester::apply("sb2-3a", "SEGMENT(0 5, 20 5)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(0 5,20 5)", strategy_ps),
                  strategy_sb);

    // Test segments below box
    tester::apply("test_b1", "SEGMENT(0 5, 9 5)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(0 5, 9 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b2", "SEGMENT(0 5, 10 5)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(0 5, 10 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b3", "SEGMENT(0 5, 11 5)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(0 5, 11 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b4", "SEGMENT(0 5, 20 5)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(0 5,20 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b5", "SEGMENT(0 5, 22 5)", box_north,
                  ps_distance<Point>("POINT(11 10)", "SEGMENT(0 5,22 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b6", "SEGMENT(10 5, 20 5)", box_north,
                  ps_distance<Point>("POINT(15 10)", "SEGMENT(10 5,20 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b7", "SEGMENT(10 5, 22 5)", box_north,
                  ps_distance<Point>("POINT(16 10)", "SEGMENT(10 5,22 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b8", "SEGMENT(12 5, 22 5)", box_north,
                  ps_distance<Point>("POINT(17 10)", "SEGMENT(12 5,22 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b9", "SEGMENT(18 5, 22 5)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(18 5,22 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b10", "SEGMENT(18 5, 24 5)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(18 5,24 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b11", "SEGMENT(20 5, 24 5)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(20 5,24 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b12", "SEGMENT(22 5, 24 5)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(22 5,24 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_b13", "SEGMENT(0 5, 125 5)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(0 5, 125 5)", strategy_ps),
                  strategy_sb);

    // Test segments above box
    tester::apply("test_a1", "SEGMENT(0 25, 9 25)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(0 25, 9 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a2", "SEGMENT(0 25, 10 25)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(0 25, 10 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a3", "SEGMENT(0 25, 11 25)", box_north,
                  ps_distance<Point>("POINT(11 20)", "SEGMENT(0 25, 11 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a4", "SEGMENT(0 25, 20 25)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(0 25, 20 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a5", "SEGMENT(0 25, 22 25)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(0 25, 22 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a6", "SEGMENT(10 25, 20 25)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(10 25, 20 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a7", "SEGMENT(10 25, 22 25)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(10 25, 22 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a8", "SEGMENT(12 25, 22 25)", box_north,
                  ps_distance<Point>("POINT(12 20)", "SEGMENT(12 25, 22 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a9", "SEGMENT(18 25, 22 25)", box_north,
                  ps_distance<Point>("POINT(18 20)", "SEGMENT(18 25, 22 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a10", "SEGMENT(18 25, 24 25)", box_north,
                  ps_distance<Point>("POINT(18 20)", "SEGMENT(18 25, 24 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a11", "SEGMENT(20 25, 24 25)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(20 25, 24 25)", strategy_ps),
                  strategy_sb);
    tester::apply("test_a12", "SEGMENT(22 25, 24 25)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(22 25, 24 25)", strategy_ps),
                  strategy_sb);

    // Segments left-right of box
    tester::apply("test_l1", "SEGMENT(0 5, 9 5)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(0 5, 9 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_l2", "SEGMENT(0 10, 9 10)", box_north,
                  ps_distance<Point>("POINT(9 10)", "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_l3", "SEGMENT(0 10, 9 15)", box_north,
                  ps_distance<Point>("POINT(9 15)", "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_l4", "SEGMENT(0 10, 0 15)", box_north,
                  ps_distance<Point>("POINT(0 15)", "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_l5", "SEGMENT(1 10, 0 15)", box_north,
                  ps_distance<Point>("POINT(1 10)", "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_l6", "SEGMENT(0 20, 9 21)", box_north,
                  ps_distance<Point>("POINT(9 21)", "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_r1", "SEGMENT(21 5, 29 5)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(21 5, 29 5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_r2", "SEGMENT(21 10, 29 10)", box_north,
                  ps_distance<Point>("POINT(21 10)", "SEGMENT(20 10, 20 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_r3", "SEGMENT(21 10, 29 15)", box_north,
                  ps_distance<Point>("POINT(21 10)", "SEGMENT(20 10, 20 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_r4", "SEGMENT(21 10, 21 15)", box_north,
                  ps_distance<Point>("POINT(21 15)", "SEGMENT(20 10, 20 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_r5", "SEGMENT(21 10, 22 15)", box_north,
                  ps_distance<Point>("POINT(21 10)", "SEGMENT(20 10, 20 20)", strategy_ps),
                  strategy_sb);
    tester::apply("test_r6", "SEGMENT(29 20, 21 21)", box_north,
                  ps_distance<Point>("POINT(21 21)", "SEGMENT(20 10, 20 20)", strategy_ps),
                  strategy_sb);

    //Segments on corners of box
    //left-top corner
    //generic
    tester::apply("test_c1", "SEGMENT(9 19.5, 11 21)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(9 19.5, 11 21)", strategy_ps),
                  strategy_sb);
    //degenerate
    tester::apply("test_c2", "SEGMENT(9 19, 11 21)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(9 19, 11 21)", strategy_ps),
                  strategy_sb);
    //left-bottom corner
    //generic
    tester::apply("test_c3", "SEGMENT(8.5 11, 11 9)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(8.5 11, 11 9)", strategy_ps),
                  strategy_sb);
    //degenerate
    tester::apply("test_c4", "SEGMENT(9 11, 11 9)", box_north,
                  0,
                  strategy_sb);
    //right-top corner
    //generic
    tester::apply("test_c5", "SEGMENT(19 21, 21 19.5)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(19 21, 21 19.5)", strategy_ps),
                  strategy_sb);
    //degenerate
    tester::apply("test_c6", "SEGMENT(19 21, 21 19)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(19 21, 21 19)", strategy_ps),
                  strategy_sb);
    //right-bottom corner
    //generic
    tester::apply("test_c7", "SEGMENT(19 9, 21 10.5)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(19 9, 21 10.5)", strategy_ps),
                  strategy_sb);
    tester::apply("test_c7", "SEGMENT(19 9, 21 11)", box_north,
                  0,
                  strategy_sb);

    //Segment and box on different hemispheres
    std::string const box_south = "BOX(10 -20,20 -10)";

    tester::apply("test_ns1", "SEGMENT(10 20, 15 30)", box_south,
                  ps_distance<Point>("POINT(10 -10)", "SEGMENT(10 20, 15 30)", strategy_ps),
                  strategy_sb);
    tester::apply("test_ns2", "SEGMENT(0 10, 12 10)", box_south,
                  pp_distance<Point>("POINT(12 10)", "POINT(12 -10)", strategy_pp),
                  strategy_sb);
    tester::apply("test_ns3", "SEGMENT(10 10, 20 10)", box_south,
                  pp_distance<Point>("POINT(10 10)", "POINT(10 -10)", strategy_pp),
                  strategy_sb);
    tester::apply("test_ns4", "SEGMENT(0 -10, 12 -10)", box_north,
                  pp_distance<Point>("POINT(12 10)", "POINT(12 -10)", strategy_pp),
                  strategy_sb);
    tester::apply("test_ns5", "SEGMENT(10 -10, 20 -10)", box_north,
                  pp_distance<Point>("POINT(10 -10)", "POINT(10 10)", strategy_pp),
                  strategy_sb);

    //Box crossing equator
    std::string const box_crossing_eq = "BOX(10 -10,20 10)";

    tester::apply("test_cr1", "SEGMENT(10 20, 15 30)", box_crossing_eq,
                  pp_distance<Point>("POINT(10 10)", "POINT(10 20)", strategy_pp),
                  strategy_sb);
    tester::apply("test_cr2", "SEGMENT(10 -20, 15 -30)", box_crossing_eq,
                  pp_distance<Point>("POINT(10 10)", "POINT(10 20)", strategy_pp),
                  strategy_sb);

    //Box crossing prime meridian

    std::string const box_crossing_mer = "BOX(-10 10,15 20)";

    tester::apply("test_cr3", "SEGMENT(-5 25, 10 30)", box_crossing_mer,
                  pp_distance<Point>("POINT(-5 25)", "POINT(-5 20)", strategy_pp),
                  strategy_sb);
    tester::apply("test_cr4", "SEGMENT(-5 5, 10 7)", box_crossing_mer,
                  pp_distance<Point>("POINT(10 7)", "POINT(10 10)", strategy_pp),
                  strategy_sb);
    tester::apply("test_cr5", "SEGMENT(-5 5, 10 5)", box_crossing_mer,
                  ps_distance<Point>("POINT(2.5 10)", "SEGMENT(-5 5, 10 5)", strategy_ps),
                  strategy_sb);


    //Geometries in south hemisphere
    tester::apply("test_south1", "SEGMENT(10 -30, 15 -30)", box_south,
                  ps_distance<Point>("POINT(10 -20)", "SEGMENT(10 -30, 15 -30)", strategy_ps),
                  strategy_sb);

    //Segments in boxes corner
    tester::apply("corner1", "SEGMENT(17 21, 25 20)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(17 21, 25 20)", strategy_ps),
                  strategy_sb);
    tester::apply("corner2", "SEGMENT(17 21, 0 20)", box_north,
                  ps_distance<Point>("POINT(10 20)", "SEGMENT(17 21, 0 20)", strategy_ps),
                  strategy_sb);
    tester::apply("corner3", "SEGMENT(17 5, 0 10)", box_north,
                  ps_distance<Point>("POINT(10 10)", "SEGMENT(17 5, 0 10)", strategy_ps),
                  strategy_sb);
    tester::apply("corner4", "SEGMENT(17 5, 25 9)", box_north,
                  ps_distance<Point>("POINT(20 10)", "SEGMENT(17 5, 25 9)", strategy_ps),
                  strategy_sb);
}

template <typename Point, typename Strategy_ps, typename Strategy_sb>
void test_distance_linestring_box(Strategy_ps const& strategy_ps, Strategy_sb const& strategy_sb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/box distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::box<Point> box_type;
    typedef test_distance_of_geometries<linestring_type, box_type> tester;

    std::string const box_north = "BOX(10 10,20 20)";

    tester::apply("sl1", "LINESTRING(0 20, 15 21, 25 19.9, 21 5, 15 5, 0 10)", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(15 21, 25 19.9)", strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("sl2", "LINESTRING(0 20, 15 21, 25 19.9, 21 5, 15 5, 15 15)", box_north,
                  0, strategy_ps, true, false, false);

    tester::apply("sl3", "LINESTRING(0 20, 15 21, 25 19.9, 21 5, 15 5, 2 20)", box_north,
                  0, strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_ps, typename Strategy_sb>
void test_distance_multi_linestring_box(Strategy_ps const& strategy_ps, Strategy_sb const& strategy_sb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multi_linestring/box distance tests" << std::endl;
#endif
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;
    typedef bg::model::box<Point> box_type;
    typedef test_distance_of_geometries<multi_linestring_type, box_type> tester;

    std::string const box_north = "BOX(10 10,20 20)";

    tester::apply("sl1", "MULTILINESTRING((0 20, 15 21, 25 19.9, 21 5, 15 5, 0 10)(25 20, 22 4, 0 0))", box_north,
                  ps_distance<Point>("POINT(20 20)", "SEGMENT(15 21, 25 19.9)", strategy_ps),
                  strategy_sb, true, false, false);
}

//===========================================================================
//===========================================================================
//===========================================================================


template <typename Point, typename Strategy_pp, typename Strategy_ps, typename Strategy_sb>
void test_all_l_ar(Strategy_pp pp_strategy, Strategy_ps ps_strategy, Strategy_sb sb_strategy)
{
    test_distance_segment_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_linestring_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_multi_linestring_polygon<Point>(pp_strategy, ps_strategy);

    test_distance_segment_multi_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_linestring_multi_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_multi_linestring_multi_polygon<Point>(pp_strategy, ps_strategy);

    test_distance_segment_ring<Point>(pp_strategy, ps_strategy);
    test_distance_linestring_ring<Point>(pp_strategy, ps_strategy);
    test_distance_multi_linestring_ring<Point>(pp_strategy, ps_strategy);

    test_distance_segment_box<Point>(pp_strategy, ps_strategy, sb_strategy);
    //test_distance_linestring_box<Point>(ps_strategy, sb_strategy);
    //test_distance_multi_linestring_box<Point>(ps_strategy, sb_strategy);

    //test_more_empty_input_linear_areal<Point>(ps_strategy);
}

BOOST_AUTO_TEST_CASE( test_all_linear_areal )
{
    typedef bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> >
                                                                    sph_point;
    test_all_l_ar<sph_point>(spherical_pp(), spherical_ps(), spherical_sb());

    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > geo_point;

    test_all_l_ar<geo_point>(vincenty_pp(), vincenty_ps(), vincenty_sb());
    test_all_l_ar<geo_point>(thomas_pp(), thomas_ps(), thomas_sb());
    test_all_l_ar<geo_point>(andoyer_pp(), andoyer_ps(), andoyer_sb());
    //test_all_l_ar<geo_point>(karney_pp(), karney_ps(), karney_sb());
}
