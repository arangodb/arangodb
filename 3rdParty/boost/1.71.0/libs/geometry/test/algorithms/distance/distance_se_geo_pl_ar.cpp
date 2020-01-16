// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_geographic_pointlike_areal
#endif

#include <boost/range.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/test/included/unit_test.hpp>
#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include "test_distance_geo_common.hpp"
#include "test_empty_geometry.hpp"

template
<
        typename Point,
        typename Strategy_pp,
        typename Strategy_ps
>
void test_distance_point_ring(Strategy_pp const& strategy_pp,
                              Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/ring distance tests" << std::endl;
#endif
    typedef bg::model::ring<Point> ring_type;
    typedef test_distance_of_geometries<Point, ring_type> tester;

    std::string const ring = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("pr1", "POINT(0 10)", ring,
                  ps_distance<Point>("POINT(0 10)",
                                     "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("pr2", "POINT(10 0)", ring,
                  pp_distance<Point>("POINT(10 10)", "POINT(10 0)", strategy_pp),
                  strategy_ps, true, true, false);
    tester::apply("pr3", "POINT(0 20)", ring,
                  0, strategy_ps, true, true, false);
    tester::apply("pr4", "POINT(10 10)", ring,
                  0, strategy_ps, true, true, false);
    tester::apply("pr4", "POINT(10 11)", ring,
                  0, strategy_ps, true, true, false);
}

//===========================================================================

template
<
        typename Point,
        typename Strategy_ps
>
void test_distance_multipoint_ring(Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/ring distance tests" << std::endl;
#endif
    typedef bg::model::ring<Point> ring_type;
    typedef bg::model::multi_point<Point> multi_point_type;
    typedef test_distance_of_geometries<multi_point_type, ring_type> tester;

    std::string const ring = "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("pr1", "MULTIPOINT(0 10,10 0,0 0)", ring,
                  ps_distance<Point>("POINT(0 10)",
                                     "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, true, false);
}


//===========================================================================

template
<
        typename Point,
        typename Strategy_pp,
        typename Strategy_ps
>
void test_distance_point_polygon(Strategy_pp const& strategy_pp,
                                 Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/polygon distance tests" << std::endl;
#endif
    typedef bg::model::polygon<Point> polygon_type;
    typedef test_distance_of_geometries<Point, polygon_type> tester;

    std::string const polygon =
            "POLYGON((10 10,0 20, 15 30, 20 15, 15 10, 10 10))";

    tester::apply("pr1", "POINT(0 10)", polygon,
                  ps_distance<Point>("POINT(0 10)",
                                     "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("pr2", "POINT(10 0)", polygon,
                  pp_distance<Point>("POINT(10 10)", "POINT(10 0)", strategy_pp),
                  strategy_ps, true, true, false);
    tester::apply("pr3", "POINT(0 20)", polygon,
                  0, strategy_ps, true, true, false);
    tester::apply("pr4", "POINT(10 10)", polygon,
                  0, strategy_ps, true, true, false);
    tester::apply("pr5", "POINT(10 10.1)", polygon,
                  0, strategy_ps, false, false, false);
}

//===========================================================================

template
<
        typename Point,
        typename Strategy_pp,
        typename Strategy_ps
>
void test_distance_multipoint_polygon(Strategy_pp const& strategy_pp,
                                      Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/polygon distance tests" << std::endl;
#endif
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_point<Point> multi_point_type;
    typedef test_distance_of_geometries<multi_point_type, polygon_type> tester;

    std::string const polygon = "POLYGON((10 10,0 20,15 30,20 15,15 10,10 10))";

    tester::apply("mpp1", "MULTIPOINT(0 10,10 0,0 0)", polygon,
                  ps_distance<Point>("POINT(0 10)",
                                     "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, true, false);

    tester::apply("mpp2", "MULTIPOINT(0 10,10 0,0 0,20.1 15)", polygon,
                  pp_distance<Point>("POINT(20 15)",
                                     "POINT(20.1 15)", strategy_pp),
                  strategy_ps, true, true, false);

    tester::apply("mpp3", "MULTIPOINT(0 10,10 0,0 0,20.1 15,10 10)", polygon,
                  0, strategy_ps, true, true, false);

    tester::apply("mpp4", "MULTIPOINT(0 10,10 0,0 0,20.1 15,10 11)", polygon,
                  0, strategy_ps, true, true, false);
}


//===========================================================================

template
<
        typename Point,
        typename Strategy_pp,
        typename Strategy_ps
>
void test_distance_point_multipolygon(Strategy_pp const& strategy_pp,
                                      Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/multipolygon distance tests" << std::endl;
#endif
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multipolygon_type;
    typedef test_distance_of_geometries<Point, multipolygon_type> tester;

    std::string const mpoly = "MULTIPOLYGON(((20 20, 20 30, 30 40, 20 20)),\
                                     ((10 10,0 20, 15 30, 20 15, 15 10, 10 10)))";

    tester::apply("pmp1", "POINT(0 10)", mpoly,
                  ps_distance<Point>("POINT(0 10)",
                                     "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, true, false);


    tester::apply("pmp2", "POINT(10 9.9)", mpoly,
                  pp_distance<Point>("POINT(10 9.9)",
                                     "POINT(10 10)", strategy_pp),
                  strategy_ps, true, true, false);

    tester::apply("pmp3", "POINT(10 11)", mpoly,
                  0, strategy_ps, true, true, false);
}

//===========================================================================

template
<
        typename Point,
        typename Strategy_pp,
        typename Strategy_ps
>
void test_distance_multipoint_multipolygon(Strategy_pp const& strategy_pp,
                                           Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/multipolygon distance tests" << std::endl;
#endif
    typedef bg::model::polygon<Point> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multipolygon_type;
    typedef bg::model::multi_point<Point> multi_point_type;
    typedef test_distance_of_geometries<multi_point_type, multipolygon_type> tester;

    std::string const mpoly = "MULTIPOLYGON(((20 20, 20 30, 30 40, 20 20)),\
                                  ((10 10,0 20, 15 30, 20 15, 15 10, 10 10)))";

    tester::apply("pr1", "MULTIPOINT(0 10,10 0,0 0)", mpoly,
                  ps_distance<Point>("POINT(0 10)",
                                     "SEGMENT(0 20, 10 10)", strategy_ps),
                  strategy_ps, true, true, false);

    tester::apply("pmp2", "MULTIPOINT(0 10,10 0,0 0,10 9.9)", mpoly,
                  pp_distance<Point>("POINT(10 9.9)",
                                     "POINT(10 10)", strategy_pp),
                  strategy_ps, true, true, false);

    tester::apply("pmp3", "MULTIPOINT(0 10,10 0,0 0,10 9.9,10 11)", mpoly,
                  0, strategy_ps, true, true, false);
}


//===========================================================================
// Cases for relative location of a point wrt to a box
//
//           |         |
//           |    3    |
//           |         |
//           +---------+
//           |         |
//       1   |    5    |   2
//           |         |
//           +---------+
//           |         |
//           |    4    |
//           |         |
//
// and also the following cases
//
//           |         |
//           A         B
//           |         |
//           +----C----+
//           |         |
//           D         E
//           |         |
//           +----F----+
//           |         |
//           G         H
//           |         |
//
// and finally we have the corners
//
//           |         |
//           |         |
//           |         |
//           a---------b
//           |         |
//           |         |
//           |         |
//           c---------d
//           |         |
//           |         |
//           |         |
//
// for each relative position we also have to test the shifted point
// (this is due to the fact that boxes have longitudes in the
// range [-180, 540)
//===========================================================================

template
<
        typename Point,
        typename Strategy_pp,
        typename Strategy_ps,
        typename Strategy_pb
>
void test_distance_point_box(Strategy_pp const& strategy_pp,
                             Strategy_ps const& strategy_ps,
                             Strategy_pb const& strategy_pb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/box distance tests" << std::endl;
#endif
    typedef bg::model::box<Point> box_type;
    typedef test_distance_of_geometries<Point, box_type> tester;

    std::string const box1 = "BOX(10 10,20 20)";

    // case 1
    tester::apply("pb1-1a", "POINT(5 25)", box1,
                  pp_distance<Point>("POINT(5 25)", "POINT(10 20)", strategy_pp),
                  strategy_pb);

    // case 1
    tester::apply("pb1-1b", "POINT(3 12)", box1,
                  ps_distance<Point>("POINT(3 12)",
                                     "SEGMENT(10 10,10 20)", strategy_ps),
                  strategy_pb);

    // case 1
    tester::apply("pb1-1c", "POINT(3 17)", box1,
                  ps_distance<Point>("POINT(3 17)",
                                     "SEGMENT(10 10,10 20)", strategy_ps),
                  strategy_pb);

    // case 1
    tester::apply("pb1-1d", "POINT(5 4)", box1,
                  pp_distance<Point>("POINT(5 4)", "POINT(10 10)", strategy_pp),
                  strategy_pb);

    // case 1
    tester::apply("pb1-1e", "POINT(-100 20)", box1,
                  pp_distance<Point>("POINT(-100 20)",
                                     "POINT(10 20)", strategy_pp),
                  strategy_pb);

    // case 1
    tester::apply("pb1-1g", "POINT(-100 10)", box1,
                  ps_distance<Point>("POINT(-100 10)",
                                     "SEGMENT(10 10,10 20)", strategy_ps),
                  strategy_pb);

    // case 2
    tester::apply("pb1-2a", "POINT(31 25)", box1,
                  pp_distance<Point>("POINT(31 25)",
                                     "POINT(20 20)", strategy_pp),
                  strategy_pb);

    // case 2
    tester::apply("pb1-2b", "POINT(23 17)", box1,
                  ps_distance<Point>("POINT(23 17)",
                                     "SEGMENT(20 10,20 20)", strategy_ps),
                  strategy_pb);

    // case 2
    tester::apply("pb1-2c", "POINT(29 3)", box1,
                  pp_distance<Point>("POINT(29 3)",
                                     "POINT(20 10)", strategy_pp),
                  strategy_pb);

    // case 2
    tester::apply("pb1-2d", "POINT(131 65)", box1,
                  pp_distance<Point>("POINT(131 65)",
                                     "POINT(20 20)", strategy_pp),
                  strategy_pb);

    // case 2
    tester::apply("pb1-2e", "POINT(110 10)", box1,
                  ps_distance<Point>("POINT(110 10)",
                                     "SEGMENT(20 10,20 20)", strategy_ps),
                  strategy_pb);

    // case 2
    tester::apply("pb1-2f", "POINT(150 20)", box1,
                  pp_distance<Point>("POINT(150 20)",
                                     "POINT(20 20)", strategy_pp),
                  strategy_pb);

    // case 3
    tester::apply("pb1-3a", "POINT(11 25)", box1,
                  pp_distance<Point>("POINT(11 25)",
                                     "POINT(11 20)", strategy_pp),
                  strategy_pb);

    // case 3
    tester::apply("pb1-3b", "POINT(15 25)", box1,
                  pp_distance<Point>("POINT(15 25)",
                                     "POINT(15 20)", strategy_pp),
                  strategy_pb);

    // case 3
    tester::apply("pb1-3c", "POINT(18 25)", box1,
                  pp_distance<Point>("POINT(18 25)",
                                     "POINT(18 20)", strategy_pp),
                  strategy_pb);

    // case 4
    tester::apply("pb1-4a", "POINT(13 4)", box1,
                  pp_distance<Point>("POINT(13 4)",
                                     "POINT(13 10)", strategy_pp),
                  strategy_pb);

    // case 4
    tester::apply("pb1-4b", "POINT(19 4)", box1,
                  pp_distance<Point>("POINT(19 4)",
                                     "POINT(19 10)", strategy_pp),
                  strategy_pb);

    // case 5
    tester::apply("pb1-5", "POINT(15 14)", box1, 0, strategy_pb);

    // case A
    tester::apply("pb1-A", "POINT(10 28)", box1,
                  pp_distance<Point>("POINT(10 28)",
                                     "POINT(10 20)", strategy_pp),
                  strategy_pb);

    // case B
    tester::apply("pb1-B", "POINT(20 28)", box1,
                  pp_distance<Point>("POINT(20 28)",
                                     "POINT(20 20)", strategy_pp),
                  strategy_pb);


    // case C
    tester::apply("pb1-C", "POINT(14 20)", box1, 0, strategy_pb);

    // case D
    tester::apply("pb1-D", "POINT(10 17)", box1, 0, strategy_pb);

    // case E
    tester::apply("pb1-E", "POINT(20 11)", box1, 0, strategy_pb);

    // case F
    tester::apply("pb1-F", "POINT(19 10)", box1, 0, strategy_pb);

    // case G
    tester::apply("pb1-G", "POINT(10 -40)", box1,
                  pp_distance<Point>("POINT(10 -40)",
                                     "POINT(10 10)", strategy_pp),
                  strategy_pb);

    // case H
    tester::apply("pb1-H", "POINT(20 -50)", box1,
                  pp_distance<Point>("POINT(20 -50)",
                                     "POINT(20 10)", strategy_pp),
                  strategy_pb);

    // case a
    tester::apply("pb1-a", "POINT(10 20)", box1, 0, strategy_pb);
    // case b
    tester::apply("pb1-b", "POINT(20 20)", box1, 0, strategy_pb);
    // case c
    tester::apply("pb1-c", "POINT(10 10)", box1, 0, strategy_pb);
    // case d
    tester::apply("pb1-d", "POINT(20 10)", box1, 0, strategy_pb);


    std::string const box2 = "BOX(170 -60,400 80)";

    // case 1 - point is closer to western meridian
    tester::apply("pb2-1a", "POINT(160 0)", box2,
                  ps_distance<Point>("POINT(160 0)",
                                     "SEGMENT(170 -60,170 80)", strategy_ps),
                  strategy_pb);

    // case 1 - point is closer to eastern meridian
    tester::apply("pb2-1b", "POINT(50 0)", box2,
                  ps_distance<Point>("POINT(50 0)",
                                     "SEGMENT(40 -60,40 80)", strategy_ps),
                  strategy_pb);

    // case 3 - equivalent point POINT(390 85) is above the box
    tester::apply("pb2-3", "POINT(30 85)", box2,
                  pp_distance<Point>("POINT(30 85)",
                                     "POINT(30 80)", strategy_pp),
                  strategy_pb);

    // case 4 - equivalent point POINT(390 -75) is below the box
    tester::apply("pb2-4", "POINT(30 -75)", box2,
                  pp_distance<Point>("POINT(30 -75)",
                                     "POINT(30 -60)", strategy_pp),
                  strategy_pb);

    // case 5 - equivalent point POINT(390 0) is inside box
    tester::apply("pb2-5", "POINT(30 0)", box2, 0, strategy_pb);


    std::string const box3 = "BOX(-150 -50,-40 70)";

    // case 1 - point is closer to western meridian
    tester::apply("pb3-1a", "POINT(-170 10)", box3,
                  ps_distance<Point>("POINT(-170 10)",
                                     "SEGMENT(-150 -50,-150 70)", strategy_ps),
                  strategy_pb);

    // case 2 - point is closer to eastern meridian
    tester::apply("pb3-2a", "POINT(5 10)", box3,
                  ps_distance<Point>("POINT(5 10)",
                                     "SEGMENT(-40 -50,-40 70)", strategy_ps),
                  strategy_pb);

    // case 2 - point is closer to western meridian
    tester::apply("pb3-2a", "POINT(160 10)", box3,
                  ps_distance<Point>("POINT(160 10)",
                                     "SEGMENT(-150 -50,-150 70)", strategy_ps),
                  strategy_pb);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb3-2c1", "POINT(85 20)", box3,
                  ps_distance<Point>("POINT(85 20)",
                                     "SEGMENT(-150 -50,-150 70)", strategy_ps),
                  strategy_pb);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb3-2c2", "POINT(85 20)", box3,
                  ps_distance<Point>("POINT(85 20)",
                                     "SEGMENT(-40 -50,-40 70)", strategy_ps),
                  strategy_pb);

    // box that is symmetric wrt the prime meridian
    std::string const box4 = "BOX(-75 -45,75 65)";

    // case 1 - point is closer to western meridian
    tester::apply("pb4-1a", "POINT(-100 10)", box4,
                  ps_distance<Point>("POINT(-100 10)",
                                     "SEGMENT(-75 -45,-75 65)", strategy_ps),
                  strategy_pb);

    // case 2 - point is closer to eastern meridian
    tester::apply("pb4-2a", "POINT(90 15)", box4,
                  ps_distance<Point>("POINT(90 15)",
                                     "SEGMENT(75 -45,75 65)", strategy_ps),
                  strategy_pb);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb4-2c1", "POINT(-180 20)", box4,
                  ps_distance<Point>("POINT(-180 20)",
                                     "SEGMENT(-75 -45,-75 65)", strategy_ps),
                  strategy_pb);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb4-2c2", "POINT(-180 20)", box4,
                  ps_distance<Point>("POINT(-180 20)",
                                     "SEGMENT(75 -45,75 65)", strategy_ps),
                  strategy_pb);


    //box degenerates to a meridian segment
    std::string const boxdeg1 = "BOX(0 10,0 20)";

    tester::apply("pbd1", "POINT(1 10)", boxdeg1,
                  ps_distance<Point>("POINT(1 10)",
                                     "SEGMENT(0 10, 0 20)", strategy_ps),
                  strategy_pb);
    tester::apply("pbd2", "POINT(1 5)", boxdeg1,
                  ps_distance<Point>("POINT(1 5)",
                                     "SEGMENT(0 10, 0 20)", strategy_ps),
                  strategy_pb);
    tester::apply("pbd3", "POINT(1 15)", boxdeg1,
                  ps_distance<Point>("POINT(1 15)",
                                     "SEGMENT(0 10, 0 20)", strategy_ps),
                  strategy_pb);
    tester::apply("pbd4", "POINT(1 25)", boxdeg1,
                  ps_distance<Point>("POINT(1 25)",
                                     "SEGMENT(0 10, 0 20)", strategy_ps),
                  strategy_pb);

    //box degenerates to a horizontal line; that is not a geodesic segment
    std::string const boxdeg2 = "BOX(10 10,20 10)";

    tester::apply("pbd5", "POINT(15 15)", boxdeg2,
                  pp_distance<Point>("POINT(15 15)",
                                     "POINT(15 10)", strategy_pp),
                  strategy_pb);
    tester::apply("pbd6", "POINT(5 15)", boxdeg2,
                  pp_distance<Point>("POINT(5 15)",
                                     "POINT(10 10)", strategy_pp),
                  strategy_pb);
    tester::apply("pbd7", "POINT(25 15)", boxdeg2,
                  pp_distance<Point>("POINT(25 15)",
                                     "POINT(20 10)", strategy_pp),
                  strategy_pb);

    //box degenerates to a point
    std::string const boxdeg3 = "BOX(0 10,0 10)";

    tester::apply("pbd8", "POINT(1 11)", boxdeg3,
                  pp_distance<Point>("POINT(1 11)",
                                     "POINT(0 10)", strategy_pp),
                  strategy_pb);
}

template
<
        typename Point,
        typename Strategy_pp,
        typename Strategy_ps,
        typename Strategy_pb
>
void test_distance_multipoint_box(Strategy_pp const& strategy_pp,
                                  Strategy_ps const& strategy_ps,
                                  Strategy_pb const& strategy_pb)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/box distance tests" << std::endl;
#endif
    typedef bg::model::box<Point> box_type;
    typedef bg::model::multi_point<Point> multi_point_type;
    typedef test_distance_of_geometries<multi_point_type, box_type> tester;

    std::string const box1 = "BOX(10 10,20 20)";

    tester::apply("mpb1", "MULTIPOINT(5 25,25 26)", box1,
                  pp_distance<Point>("POINT(5 25)", "POINT(10 20)", strategy_pp),
                  strategy_pb, true, false, false);

    tester::apply("mpb2", "MULTIPOINT(110 10,110 9,110 0)", box1,
                  ps_distance<Point>("POINT(110 10)",
                                     "SEGMENT(20 10,20 20)", strategy_ps),
                  strategy_pb, true, false, false);

    tester::apply("mpb3", "MULTIPOINT(110 10,110 9,110 0,10 20)", box1,
                  0, strategy_pb, true, false, false);

    tester::apply("mpb3", "MULTIPOINT(110 10,110 9,110 0,15 15)", box1,
                  0, strategy_pb, true, false, false);
}

//===========================================================================
//===========================================================================
//===========================================================================

template
<
    typename Point,
    typename Strategy_pp,
    typename Strategy_ps,
    typename Strategy_pb
>
void test_all_pl_ar(Strategy_pp pp_strategy,
                    Strategy_ps ps_strategy,
                    Strategy_pb pb_strategy)
{
    test_distance_point_ring<Point>(pp_strategy, ps_strategy);
    test_distance_multipoint_ring<Point>(ps_strategy);

    test_distance_point_polygon<Point>(pp_strategy, ps_strategy);
    test_distance_multipoint_polygon<Point>(pp_strategy, ps_strategy);

    test_distance_point_multipolygon<Point>(pp_strategy, ps_strategy);
    test_distance_multipoint_multipolygon<Point>(pp_strategy, ps_strategy);

    test_distance_point_box<Point>(pp_strategy, ps_strategy, pb_strategy);
    test_distance_multipoint_box<Point>(pp_strategy, ps_strategy, pb_strategy);

    test_more_empty_input_pointlike_areal<Point>(ps_strategy);
}

BOOST_AUTO_TEST_CASE( test_all_pointlike_areal )
{
    typedef bg::model::point
            <
                double, 2,
                bg::cs::spherical_equatorial<bg::degree>
            > sph_point;

    test_all_pl_ar<sph_point>(spherical_pp(), spherical_ps(), spherical_pb());

    typedef bg::model::point
            <
                double, 2,
                bg::cs::geographic<bg::degree>
            > geo_point;

    test_all_pl_ar<geo_point>(vincenty_pp(), vincenty_ps(), vincenty_pb());
    test_all_pl_ar<geo_point>(thomas_pp(), thomas_ps(), thomas_pb());
    test_all_pl_ar<geo_point>(andoyer_pp(), andoyer_ps(), andoyer_pb());

    // test with different spheroid
    stype spheroid(6372000, 6370000);
    test_all_pl_ar<geo_point>(andoyer_pp(spheroid), andoyer_ps(spheroid),  andoyer_pb(spheroid));
}
