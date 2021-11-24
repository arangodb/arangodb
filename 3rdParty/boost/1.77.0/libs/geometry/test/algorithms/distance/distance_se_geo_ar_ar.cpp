// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017-2020 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_geographic_areal_areal
#endif

#include <boost/type_traits/is_same.hpp>

#include <boost/test/included/unit_test.hpp>
#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include "test_distance_geo_common.hpp"
#include "test_empty_geometry.hpp"

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_ring_ring(Strategy_pp const& strategy_pp,
                             Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "ring/ring distance tests" << std::endl;
#endif
    typedef bg::model::ring<Point> ring_type;

    typedef test_distance_of_geometries<ring_type, ring_type> tester;

    std::string const ring = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))";

    tester::apply("rr1", ring, "POLYGON((16 0,13 0,15 1,16 0))",
                  ps_distance<Point>("POINT(13 1)",
                                     "SEGMENT(13 0,15 1)", strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("rr2", ring, "POLYGON((16 0,14 1,15 1,16 0))",
                  pp_distance<Point>("POINT(13 1)", "POINT(14 1)", strategy_pp),
                  strategy_ps, true, false, false);

    tester::apply("rr3", ring, ring,
                  0, strategy_ps, true, false, false);

}

//============================================================================

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_ring_polygon(Strategy_pp const& strategy_pp,
                                Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "ring/polygon distance tests" << std::endl;
#endif
    typedef bg::model::ring<Point> ring_type;
    typedef bg::model::polygon<Point> polygon_type;

    typedef test_distance_of_geometries<ring_type, polygon_type> tester;

    std::string const ring = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))";

    tester::apply("rp1", ring, "POLYGON((16 0,13 0,15 1,16 0))",
                  ps_distance<Point>("POINT(13 1)",
                                     "SEGMENT(13 0,15 1)", strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("rp2", ring, "POLYGON((16 0,14 1,15 1,16 0))",
                  pp_distance<Point>("POINT(13 1)", "POINT(14 1)", strategy_pp),
                  strategy_ps, true, false, false);

    tester::apply("rp3", ring, "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))",
                  0, strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_polygon_polygon(Strategy_pp const& strategy_pp,
                                   Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "polygon/polygon distance tests" << std::endl;
#endif
    typedef bg::model::polygon<Point> polygon_type;

    typedef test_distance_of_geometries<polygon_type, polygon_type> tester;

    std::string const poly = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))";

    tester::apply("pp1", poly, "POLYGON((16 0,13 0,15 1,16 0))",
                  ps_distance<Point>("POINT(13 1)",
                                     "SEGMENT(13 0,15 1)", strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("pp2", poly, "POLYGON((16 0,14 1,15 1,16 0))",
                  pp_distance<Point>("POINT(13 1)", "POINT(14 1)", strategy_pp),
                  strategy_ps, true, false, false);

    tester::apply("pp3", poly, "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))",
                  0, strategy_ps, true, false, false);

}

//============================================================================

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_ring_multi_polygon(Strategy_pp const& strategy_pp,
                                      Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "ring/multi_polygon distance tests" << std::endl;
#endif
    typedef bg::model::ring<Point> ring_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;

    typedef test_distance_of_geometries<ring_type, multi_polygon_type> tester;

    std::string const ring = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))";

    tester::apply("rmp1", ring, "MULTIPOLYGON(((16 0,13 0,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  ps_distance<Point>("POINT(12.5 2.5)", "SEGMENT(12 3,13 1)",
                                     strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("rmp2", ring, "MULTIPOLYGON(((16 0,13.1 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  pp_distance<Point>("POINT(13 1)", "POINT(13.1 1)", strategy_pp),
                  strategy_ps, true, false, false);

    tester::apply("rmp3", ring, "MULTIPOLYGON(((16 0,13 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);

    tester::apply("rmp4", ring, "MULTIPOLYGON(((16 0,12 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);
}

template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_polygon_multi_polygon(Strategy_pp const& strategy_pp,
                                         Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "polygon/multi_polygon distance tests" << std::endl;
#endif
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;

    typedef test_distance_of_geometries<polygon_type, multi_polygon_type> tester;

    std::string const poly = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))";

    tester::apply("pmp1", poly, "MULTIPOLYGON(((16 0,13 0,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  ps_distance<Point>("POINT(12.5 2.5)", "SEGMENT(12 3,13 1)",
                                     strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("pmp2", poly, "MULTIPOLYGON(((16 0,13.1 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  pp_distance<Point>("POINT(13 1)", "POINT(13.1 1)", strategy_pp),
                  strategy_ps, true, false, false);

    tester::apply("pmp3", poly, "MULTIPOLYGON(((16 0,13 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);

    tester::apply("pmp4", poly, "MULTIPOLYGON(((16 0,12 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);

    // w/ interior ring
    std::string const poly_interior = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0),\
                                               (12 1,11 1,12 2,12 1))";

    tester::apply("pmp1", poly_interior, "MULTIPOLYGON(((16 0,13 0,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  ps_distance<Point>("POINT(12.5 2.5)", "SEGMENT(12 3,13 1)",
                                     strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("pmp2", poly_interior, "MULTIPOLYGON(((16 0,13.1 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  pp_distance<Point>("POINT(13 1)", "POINT(13.1 1)", strategy_pp),
                  strategy_ps, true, false, false);

    tester::apply("pmp3", poly_interior, "MULTIPOLYGON(((16 0,13 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);

    tester::apply("pmp4", poly_interior, "MULTIPOLYGON(((16 0,12 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);

}


template <typename Point, typename Strategy_pp, typename Strategy_ps>
void test_distance_multi_polygon_multi_polygon(Strategy_pp const& strategy_pp,
                                               Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multi_polygon/multi_polygon distance tests" << std::endl;
#endif
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;

    typedef test_distance_of_geometries<multi_polygon_type, multi_polygon_type>
            tester;

    std::string const mpoly = "MULTIPOLYGON(((11 0,10 1,11 2,12 3,13 1,11 0)),\
                                            ((0 0,0 1,1 1,1 0,0 0)))";

    tester::apply("mpmp1", mpoly, "MULTIPOLYGON(((16 0,13 0,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  ps_distance<Point>("POINT(12.5 2.5)", "SEGMENT(12 3,13 1)",
                                     strategy_ps),
                  strategy_ps, true, false, false);

    tester::apply("mpmp2", mpoly, "MULTIPOLYGON(((16 0,13.1 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  pp_distance<Point>("POINT(13 1)", "POINT(13.1 1)", strategy_pp),
                  strategy_ps, true, false, false);

    tester::apply("mpmp3", mpoly, "MULTIPOLYGON(((16 0,13 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);

    tester::apply("mpmp4", mpoly, "MULTIPOLYGON(((16 0,12 1,15 1,16 0)),\
                                ((12.5 2.5,12.5 4,14 2.5,12.5 2.5)))",
                  0, strategy_ps, true, false, false);

}

//============================================================================

template
<
    typename Point,
    typename Strategy_pp,
    typename Strategy_ps,
    typename Strategy_sb
>
void test_distance_ring_box(Strategy_pp const& strategy_pp,
                            Strategy_ps const& strategy_ps,
                            Strategy_sb const& strategy_sb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "ring/box distance tests" << std::endl;
#endif
    typedef bg::model::box<Point> box_type;
    typedef bg::model::ring<Point> ring_type;

    typedef test_distance_of_geometries<ring_type, box_type> tester;

    std::string const ring = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))";

    tester::apply("rb1", ring, "BOX(10 10,20 20)",
                  sb_distance<Point>("SEGMENT(11 2,12 3)",
                                     "BOX(10 10,20 20)", strategy_sb),
                  strategy_sb, true, false, false);

    tester::apply("rb2", ring, "BOX(17 0,20 3)",
                  ps_distance<Point>("POINT(13 1)",
                                     "SEGMENT(17 0,17 3)", strategy_ps),
                  strategy_sb, true, false, false);

    tester::apply("rb3", ring, "BOX(17 0,20 1)",
                  pp_distance<Point>("POINT(17 1)", "POINT(13 1)", strategy_pp),
                  strategy_sb, true, false, false);

    tester::apply("rb4", ring, "BOX(12 0,20 1)",
                  0, strategy_sb, true, false, false);
}

template
<
    typename Point,
    typename Strategy_pp,
    typename Strategy_ps,
    typename Strategy_sb
>
void test_distance_polygon_box(Strategy_pp const& strategy_pp,
                               Strategy_ps const& strategy_ps,
                               Strategy_sb const& strategy_sb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "polygon/box distance tests" << std::endl;
#endif
    typedef bg::model::box<Point> box_type;
    typedef bg::model::polygon<Point> polygon_type;

    typedef test_distance_of_geometries<polygon_type, box_type> tester;

    std::string const polygon = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0))";

    tester::apply("pb1", polygon, "BOX(10 10,20 20)",
                  sb_distance<Point>("SEGMENT(11 2,12 3)",
                                     "BOX(10 10,20 20)", strategy_sb),
                  strategy_sb, true, false, false);

    tester::apply("pb2", polygon, "BOX(17 0,20 3)",
                  ps_distance<Point>("POINT(13 1)",
                                     "SEGMENT(17 0,17 3)", strategy_ps),
                  strategy_sb, true, false, false);

    tester::apply("pb3", polygon, "BOX(17 0,20 1)",
                  pp_distance<Point>("POINT(17 1)", "POINT(13 1)", strategy_pp),
                  strategy_sb, true, false, false);

    tester::apply("pb4", polygon, "BOX(12 0,20 1)",
                  0, strategy_sb, true, false, false);

    // w/ interior ring
    std::string const poly_interior = "POLYGON((11 0,10 1,11 2,12 3,13 1,11 0),\
                                               (12 1,11 1,12 2,12 1))";

    tester::apply("pb5", poly_interior, "BOX(10 10,20 20)",
                  sb_distance<Point>("SEGMENT(11 2,12 3)",
                                     "BOX(10 10,20 20)", strategy_sb),
                  strategy_sb, true, false, false);
}

template
<
    typename Point,
    typename Strategy_pp,
    typename Strategy_ps,
    typename Strategy_sb
>
void test_distance_multi_polygon_box(Strategy_pp const& strategy_pp,
                                     Strategy_ps const& strategy_ps,
                                     Strategy_sb const& strategy_sb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multi_polygon/box distance tests" << std::endl;
#endif
    typedef bg::model::box<Point> box_type;
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;

    typedef test_distance_of_geometries<multi_polygon_type, box_type> tester;

    std::string const multi_polygon = "MULTIPOLYGON(((20 20,20 30,30 40,20 20)),\
                                       ((10 10,0 20,15 30,20 15,15 10,10 10)))";

    tester::apply("mpb1", multi_polygon, "BOX(0 0,5 5)",
                  sb_distance<Point>("SEGMENT(10 10,0 20)",
                                     "BOX(0 0,5 5)", strategy_sb),
                  strategy_sb, true, false, false);

    tester::apply("mpb2", multi_polygon, "BOX(27 0,30 16)",
                  ps_distance<Point>("POINT(20 15)",
                                     "SEGMENT(27 0,27 16)", strategy_ps),
                  strategy_sb, true, false, false);

    tester::apply("mpb3", multi_polygon, "BOX(27 0,30 15)",
                  pp_distance<Point>("POINT(20 15)",
                                     "POINT(27 15)", strategy_pp),
                  strategy_sb, true, false, false);

    tester::apply("mpb4", multi_polygon, "BOX(17 0,20 14)",
                  0, strategy_sb, true, false, false);
}


//===========================================================================
// Cases for relative location of box2 wrt to box1
//
//           |         |
//      11   |    7    |   4
//           |         |
//    --10---+---------+---3---
//           |         |
//       9   |    6    |   2
//           |         |
//    -------+---------+-------
//           |         |
//       8   |    5    |   1
//           |         |
//
// case 6 includes all possible intersections
// The picture assumes northern hemisphere location
// southern hemisphere picture is mirrored wrt the equator


template
<
    typename Point,
    typename Strategy_pp,
    typename Strategy_ps,
    typename Strategy_bb
>
void test_distance_box_box(Strategy_pp const& strategy_pp,
                           Strategy_ps const& strategy_ps,
                           Strategy_bb const& strategy_bb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "box/box distance tests" << std::endl;
#endif
    typedef bg::model::box<Point> box_type;

    typedef test_distance_of_geometries<box_type, box_type> tester;

    std::string const box1 = "BOX(10 10,20 20)";

    // case 1
    tester::apply("bb1", box1, "BOX(30 0,40 5)",
                  pp_distance<Point>("POINT(20 10)", "POINT(30 5)", strategy_pp),
                  strategy_bb);

    // case 2
    tester::apply("bb2-a", box1, "BOX(30 12, 40 17)",
                  ps_distance<Point>("POINT(30 17)",
                                     "SEGMENT(20 10,20 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb2-b", box1, "BOX(30 10, 40 17)",
                  ps_distance<Point>("POINT(30 17)",
                                     "SEGMENT(20 10,20 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb2-c", box1, "BOX(30 8, 40 17)",
                  ps_distance<Point>("POINT(30 17)",
                                     "SEGMENT(20 10,20 20)", strategy_ps),
                  strategy_bb);


    // case 3
    tester::apply("bb3-a", box1, "BOX(30 15, 40 25)",
                  ps_distance<Point>("POINT(20 20)",
                                     "SEGMENT(30 15,30 25)", strategy_ps),
                  strategy_bb);

    tester::apply("bb3-b", box1, "BOX(30 20, 40 40)",
                  ps_distance<Point>("POINT(20 20)",
                                     "SEGMENT(30 20,30 40)", strategy_ps),
                  strategy_bb);

    // case 4
    tester::apply("bb4", box1, "BOX(30 25, 40 40)",
                  pp_distance<Point>("POINT(20 20)",
                                     "POINT(30 25)", strategy_pp),
                  strategy_bb);

    // case 5
    tester::apply("bb5", box1, "BOX(12 2, 17 7)",
                  pp_distance<Point>("POINT(17 7)", "POINT(17 10)", strategy_pp),
                  strategy_bb);

    // case 6, boxes intersect thus distance is 0
    tester::apply("bb6-a", box1, "BOX(12 2, 17 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-b", box1, "BOX(12 2, 17 17)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-c", box1, "BOX(20 2, 30 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-d", box1, "BOX(20 11, 30 15)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-e", box1, "BOX(20 20, 30 30)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-f", box1, "BOX(15 20, 17 30)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-g", box1, "BOX(8 20, 10 25)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-h", box1, "BOX(8 15 , 10 17)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-i", box1, "BOX(8 8, 10 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-j", box1, "BOX(15 8, 17 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    // case 7
    tester::apply("bb7", box1, "BOX(12 22, 17 27)",
                  pp_distance<Point>("POINT(17 20)",
                                     "POINT(17 22)", strategy_pp),
                  strategy_bb);

    // case 8
    tester::apply("bb8", box1, "BOX(4 4, 8 8)",
                  pp_distance<Point>("POINT(8 8)", "POINT(10 10)", strategy_pp),
                  strategy_bb);

    // case 9
    tester::apply("bb9-a", box1, "BOX(4 14, 8 18)",
                  ps_distance<Point>("POINT(8 18)",
                                     "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb9-b", box1, "BOX(4 10, 8 18)",
                  ps_distance<Point>("POINT(8 18)",
                                     "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb9-c", box1, "BOX(4 8, 8 18)",
                  ps_distance<Point>("POINT(8 18)",
                                     "SEGMENT(10 10, 10 20)", strategy_ps),
                  strategy_bb);

    // case 10
    tester::apply("bb10a", box1, "BOX(4 18, 8 22)",
                  ps_distance<Point>("POINT(10 20)",
                                     "SEGMENT(8 18, 8 22)", strategy_ps),
                  strategy_bb);

    std::string const box1m = "BOX(10 -20,20 -10)";
    tester::apply("bb10am", box1m, "BOX(4 -22, 8 -18)",
                  ps_distance<Point>("POINT(10 20)",
                                     "SEGMENT(8 18, 8 22)", strategy_ps),
                  strategy_bb);

    tester::apply("bb10b", box1, "BOX(4 20, 8 22)",
                  ps_distance<Point>("POINT(10 20)",
                                     "SEGMENT(8 20, 8 22)", strategy_ps),
                  strategy_bb);

    tester::apply("bb10bm", box1m, "BOX(4 -22, 8 -20)",
                  ps_distance<Point>("POINT(10 20)",
                                     "SEGMENT(8 22, 8 20)", strategy_ps),
                  strategy_bb);

    // case 11
    tester::apply("bb11", box1, "BOX(4 22, 8 24)",
                  pp_distance<Point>("POINT(8 22)", "POINT(10 20)", strategy_pp),
                  strategy_bb);

    // far away boxes
    tester::apply("bb-far", "BOX(150 15, 170 25)", box1,
                  ps_distance<Point>("POINT(20 20)",
                                     "SEGMENT(150 15, 150 25)", strategy_ps),
                  strategy_bb);

    // crosses antimeridian
    tester::apply("bb-anti1", "BOX(170 15, -160 25)", box1,
                  ps_distance<Point>("POINT(20 20)",
                                     "SEGMENT(170 15, 170 25)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-anti2", "BOX(170 15, -160 25)", "BOX(160 10, -170 20)",
                  pp_distance<Point>("POINT(20 20)",
                                     "POINT(20 20)", strategy_pp),
                  strategy_bb);

    tester::apply("bb-anti3", "BOX(170 15, -160 25)", "BOX(160 10, 170 20)",
                  pp_distance<Point>("POINT(20 20)",
                                     "POINT(20 20)", strategy_pp),
                  strategy_bb);

    tester::apply("bb-anti4", "BOX(170 10, -160 20)", "BOX(160 30, -170 40)",
                  pp_distance<Point>("POINT(180 20)",
                                     "POINT(180 30)", strategy_pp),
                  strategy_bb);

    // South hemisphere

    tester::apply("bb-south1", "BOX(10 -20, 20 -10)", "BOX(30 -15, 40 -12)",
                  ps_distance<Point>("POINT(30 -15)",
                                     "SEGMENT(20 -10, 20 -20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-south2", "BOX(10 -20, 20 -10)", "BOX(30 -30, 40 -25)",
                  pp_distance<Point>("POINT(30 -25)",
                                     "POINT(20 -20)", strategy_pp),
                  strategy_bb);

    tester::apply("bb-south3", "BOX(10 -20, 20 -10)", "BOX(30 -25, 40 -15)",
                  ps_distance<Point>("POINT(20 -20)",
                                     "SEGMENT(30 -15, 30 -25)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-south4", "BOX(10 -20, 20 -10)", "BOX(5 -30, 30 -25)",
                  pp_distance<Point>("POINT(10 -25)",
                                     "POINT(10 -20)", strategy_pp),
                  strategy_bb);

    tester::apply("bb-south4", "BOX(10 -20, 20 -10)", "BOX(5 -7, 30 -5)",
                  pp_distance<Point>("POINT(10 -7)",
                                     "POINT(10 -10)", strategy_pp),
                  strategy_bb);


    // Crosses equator

    tester::apply("bb-eq1", "BOX(30 -15, 40 30)", "BOX(10 -20, 20 25)",
                  ps_distance<Point>("POINT(20 25)",
                                     "SEGMENT(30 -15, 30 30)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-eq1b", "BOX(30 -15, 40 30)", "BOX(10 -20, 20 10)",
                  ps_distance<Point>("POINT(30 -15)",
                                     "SEGMENT(20 10, 20 -20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-eq1bm", "BOX(30 -30, 40 15)", "BOX(10 -10, 20 20)",
                  ps_distance<Point>("POINT(30 15)",
                                     "SEGMENT(20 -10, 20 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-eq2", "BOX(30 -15, 40 20)", "BOX(10 -20, 20 25)",
                  ps_distance<Point>("POINT(30 20)",
                                     "SEGMENT(20 -20, 20 25)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-eq3", "BOX(30 5, 40 20)", "BOX(10 -20, 20 25)",
                  ps_distance<Point>("POINT(30 20)",
                                     "SEGMENT(20 -20, 20 25)", strategy_ps),
                  strategy_bb);

    tester::apply("bb-eq4", "BOX(5 -30, 40 -25)", "BOX(10 -20, 20 25)",
                  pp_distance<Point>("POINT(10 -25)",
                                     "POINT(10 -20)", strategy_pp),
                  strategy_bb);

    tester::apply("bb-eq5", "BOX(30 5, 40 20)", "BOX(10 -20, 50 25)",
                  pp_distance<Point>("POINT(30 20)",
                                     "POINT(30 20)", strategy_pp),
                  strategy_bb);

    tester::apply("bb-eq6", "BOX(30 5, 40 20)", "BOX(10 -20, 35 25)",
                  pp_distance<Point>("POINT(30 20)",
                                     "POINT(30 20)", strategy_pp),
                  strategy_bb);

    // One box in the north and one in the south hemisphere

    tester::apply("bb-ns1", "BOX(30 15, 40 20)", "BOX(10 -20, 20 -15)",
                  pp_distance<Point>("POINT(30 15)",
                                     "POINT(20 -15)", strategy_pp),
                  strategy_bb);

    tester::apply("bb-ns2", "BOX(30 15, 40 20)", "BOX(25 -20, 50 -15)",
                  pp_distance<Point>("POINT(30 15)",
                                     "POINT(30 -15)", strategy_pp),
                  strategy_bb);

    //negative coordinates

    std::string const box1neg = "BOX(-20 10,-10 20)";

    // case 1
    tester::apply("bb1", box1neg, "BOX(-40 0,-30 5)",
                  pp_distance<Point>("POINT(-20 10)",
                                     "POINT(-30 5)", strategy_pp),
                  strategy_bb);

    // case 2
    tester::apply("bb2-a", box1neg, "BOX(-40 12, -30 17)",
                  ps_distance<Point>("POINT(-30 17)",
                                     "SEGMENT(-20 10,-20 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb2-b", box1neg, "BOX(-40 10, -30 17)",
                  ps_distance<Point>("POINT(-30 17)",
                                     "SEGMENT(-20 10,-20 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb2-c", box1neg, "BOX(-40 8, -30 17)",
                  ps_distance<Point>("POINT(-30 17)",
                                     "SEGMENT(-20 10,-20 20)", strategy_ps),
                  strategy_bb);


    // case 3
    tester::apply("bb3-a", box1neg, "BOX(-40 15, -30 25)",
                  ps_distance<Point>("POINT(-20 20)",
                                     "SEGMENT(-30 15,-30 25)", strategy_ps),
                  strategy_bb);

    tester::apply("bb3-b", box1neg, "BOX(-40 20, -30 40)",
                  ps_distance<Point>("POINT(-20 20)",
                                     "SEGMENT(-30 20,-30 40)", strategy_ps),
                  strategy_bb);

    // case 4
    tester::apply("bb4", box1neg, "BOX(-40 25, -30 40)",
                  pp_distance<Point>("POINT(-20 20)",
                                     "POINT(-30 25)", strategy_pp),
                  strategy_bb);

    // case 5
    tester::apply("bb5", box1neg, "BOX(-17 2,-12 7)",
                  pp_distance<Point>("POINT(-17 7)",
                                     "POINT(-17 10)", strategy_pp),
                  strategy_bb);

    // case 6, boxes intersect thus distance is 0
    tester::apply("bb6-a", box1neg, "BOX(-17 2, -12 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-b", box1neg, "BOX(-17 2, -12 17)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-c", box1neg, "BOX(-30 2, -20 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-d", box1neg, "BOX(-30 11, -20 15)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-e", box1neg, "BOX(-30 20, -20 30)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-f", box1neg, "BOX(-17 20, -15 30)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-g", box1neg, "BOX(-10 20, -8 25)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-h", box1neg, "BOX(-10 15 , -8 17)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-i", box1neg, "BOX(-10 8, -8 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    tester::apply("bb6-j", box1neg, "BOX(-17 8, -15 10)",
                  pp_distance<Point>("POINT(0 0)", "POINT(0 0)", strategy_pp),
                  strategy_bb);

    // case 7
    tester::apply("bb7", box1neg, "BOX(-17 22, -12 27)",
                  pp_distance<Point>("POINT(-17 20)",
                                     "POINT(-17 22)", strategy_pp),
                  strategy_bb);

    // case 8
    tester::apply("bb8", box1neg, "BOX(-8 4, -4 8)",
                  pp_distance<Point>("POINT(-8 8)",
                                     "POINT(-10 10)", strategy_pp),
                  strategy_bb);

    // case 9
    tester::apply("bb9-a", box1neg, "BOX(-8 14, -4 18)",
                  ps_distance<Point>("POINT(-8 18)",
                                     "SEGMENT(-10 10, -10 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb9-b", box1neg, "BOX(-8 10, -4 18)",
                  ps_distance<Point>("POINT(-8 18)",
                                     "SEGMENT(-10 10, -10 20)", strategy_ps),
                  strategy_bb);

    tester::apply("bb9-c", box1neg, "BOX(-8 8, -4 18)",
                  ps_distance<Point>("POINT(-8 18)",
                                     "SEGMENT(-10 10, -10 20)", strategy_ps),
                  strategy_bb);

    // case 10
    tester::apply("bb10", box1neg, "BOX(-8 18, -4 22)",
                  ps_distance<Point>("POINT(-10 20)",
                                     "SEGMENT(-8 18, -8 22)", strategy_ps),
                  strategy_bb);

    tester::apply("bb10", box1neg, "BOX(-8 20, -4 22)",
                  ps_distance<Point>("POINT(-10 20)",
                                     "SEGMENT(-8 20, -8 22)", strategy_ps),
                  strategy_bb);

    // case 11
    tester::apply("bb11", box1neg, "BOX(-8 22, -4 24)",
                  pp_distance<Point>("POINT(-8 22)",
                                     "POINT(-10 20)", strategy_pp),
                  strategy_bb);


    //Degenerate cases

    //1st box degenerates to a meridian segment
    std::string const box1deg = "BOX(0 10,0 20)";

    //2nd box generic
    tester::apply("pbd1", box1deg, "BOX(1 15, 2 25)",
                  ps_distance<Point>("POINT(0 20)",
                                     "SEGMENT(1 15, 1 25)", strategy_ps),
                  strategy_bb);

    //2nd box degenerates to a meridian segment
    tester::apply("pbd2", box1deg, "BOX(1 15, 1 25)",
                  ps_distance<Point>("POINT(0 20)",
                                     "SEGMENT(1 15, 1 25)", strategy_ps),
                  strategy_bb);

    //2nd box degenerates to a horizontal line
    //test fails for thomas strategy; test only for andoyer
    tester::apply("pbd3", box1deg, "BOX(1 15, 2 15)",
                  pp_distance<Point>("POINT(1 15)",
                                     "POINT(0 15)", andoyer_pp()),
                  andoyer_bb());

    //2nd box degenerates to a point
    tester::apply("pbd4", box1deg, "BOX(1 15, 1 15)",
                  ps_distance<Point>("POINT(1 15)",
                                     "SEGMENT(0 10, 0 20)", strategy_ps),
                  strategy_bb);

    //---
    //1st box degenerates to a horizontal line; that is not a geodesic segment
    std::string const box2deg = "BOX(10 10,20 10)";

    //2nd box generic
    tester::apply("pbd5", box2deg, "BOX(15 15, 25 20)",
                  pp_distance<Point>("POINT(15 15)",
                                     "POINT(15 10)", strategy_pp),
                  strategy_bb);

    //2nd box degenerates to a horizontal line
    tester::apply("pbd6", box2deg, "BOX(15 15, 25 15)",
                  pp_distance<Point>("POINT(15 15)",
                                     "POINT(15 10)", strategy_pp),
                  strategy_bb);

    //2nd box degenerates to a point
    tester::apply("pbd7", box2deg, "BOX(15 15, 15 15)",
                  pp_distance<Point>("POINT(15 15)",
                                     "POINT(15 10)", strategy_pp),
                  strategy_bb);

    //---
    //1st box degenerates to a point
    std::string const box3deg = "BOX(0 6,0 6)";

    //2nd box generic
    tester::apply("pbd8", box3deg, "BOX(15 15, 25 20)",
                  ps_distance<Point>("POINT(0 6)",
                                     "SEGMENT(15 15, 15 20)", strategy_ps),
                  strategy_bb);

    //2nd box degenerates to a point
    tester::apply("pbd9", box3deg, "BOX(15 15, 15 15)",
                  pp_distance<Point>("POINT(0 6)",
                                     "POINT(15 15)", strategy_pp),
                  strategy_bb);
}

//===========================================================================

template
<
    typename Point,
    typename Strategy_pp,
    typename Strategy_ps,
    typename Strategy_bb,
    typename Strategy_sb
>
void test_all_ar_ar(Strategy_pp pp_strategy,
                    Strategy_ps ps_strategy,
                    Strategy_bb bb_strategy,
                    Strategy_sb sb_strategy)
{
    test_distance_ring_ring<Point>(pp_strategy, ps_strategy);

    test_distance_ring_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_polygon_polygon<Point>(pp_strategy, ps_strategy);

    test_distance_ring_multi_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_polygon_multi_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_multi_polygon_multi_polygon<Point>(pp_strategy, ps_strategy);

    test_distance_polygon_box<Point>(pp_strategy, ps_strategy, sb_strategy);
    test_distance_multi_polygon_box<Point>(pp_strategy, ps_strategy, sb_strategy);
    test_distance_ring_box<Point>(pp_strategy, ps_strategy, sb_strategy);
    test_distance_box_box<Point>(pp_strategy, ps_strategy, bb_strategy);

    test_more_empty_input_areal_areal<Point>(ps_strategy);
}

BOOST_AUTO_TEST_CASE( test_all_areal_areal )
{
    typedef bg::model::point
            <
                double, 2,
                bg::cs::spherical_equatorial<bg::degree>
            > sph_point;

    test_all_ar_ar<sph_point>(spherical_pp(), spherical_ps(), spherical_bb(), spherical_sb());

    typedef bg::model::point
            <
                double, 2,
                bg::cs::geographic<bg::degree>
            > geo_point;

    test_all_ar_ar<geo_point>(vincenty_pp(), vincenty_ps(), vincenty_bb(), vincenty_sb());
    test_all_ar_ar<geo_point>(thomas_pp(), thomas_ps(), thomas_bb(), thomas_sb());
    test_all_ar_ar<geo_point>(andoyer_pp(), andoyer_ps(), andoyer_bb(), andoyer_sb());
    //test_all_ar_ar<geo_point>(karney_pp(), karney_ps(), karney_bb(), karney_sb());

    // test with different spheroid
    stype spheroid(6372000, 6370000);
    test_all_ar_ar<geo_point>(andoyer_pp(spheroid), andoyer_ps(spheroid),
                              andoyer_bb(spheroid), andoyer_sb(spheroid));
}
