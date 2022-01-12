// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017, 2018, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_geographic_pointlike_pointlike
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_geo_common.hpp"
#include "test_empty_geometry.hpp"

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_point_point(Strategy const& strategy)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/point distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<Point, Point> tester;

    tester::apply("p-p-01",
                  "POINT(1 1)",
                  "POINT(0 0)",
                  strategy.apply(Point(1,1), Point(0,0)),
                  strategy, true, false, false);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_multipoint_point(Strategy const& strategy)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/point distance tests" << std::endl;
#endif
    typedef bg::model::multi_point<Point> multi_point_type;

    typedef test_distance_of_geometries<multi_point_type, Point> tester;

    tester::apply("mp-p-01",
                  "MULTIPOINT(1 1,1 2,2 3)",
                  "POINT(0 0)",
                  pp_distance<Point>("POINT(0 0)","POINT(1 1)",strategy),
                  strategy, true, false, false);

    tester::apply("mp-p-01",
                  "MULTIPOINT(0 0,0 2,2 0,2 2)",
                  "POINT(1.1 1.1)",
                  pp_distance<Point>("POINT(1.1 1.1)","POINT(2 2)",strategy),
                  strategy, true, false, false);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_multipoint_multipoint(Strategy const& strategy)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/multipoint distance tests" << std::endl;
#endif
    typedef bg::model::multi_point<Point> multi_point_type;

    typedef test_distance_of_geometries<multi_point_type, multi_point_type> tester;

    tester::apply("mp-mp-01",
                  "MULTIPOINT(1 1,1 2,2 3)",
                  "MULTIPOINT(0 0, 0 -1)",
                  pp_distance<Point>("POINT(0 0)","POINT(1 1)",strategy),
                  strategy, true, false, false);
}

//===========================================================================
//===========================================================================
//===========================================================================

template <typename Point, typename Strategy>
void test_all_pl_pl(Strategy pp_strategy)
{
    test_distance_point_point<Point>(pp_strategy);
    test_distance_multipoint_point<Point>(pp_strategy);
    test_distance_multipoint_multipoint<Point>(pp_strategy);

    test_more_empty_input_pointlike_pointlike<Point>(pp_strategy);
}

BOOST_AUTO_TEST_CASE( test_all_pointlike_pointlike )
{
    typedef bg::model::point
            <
                double, 2,
                bg::cs::spherical_equatorial<bg::degree>
            > sph_point;

    test_all_pl_pl<sph_point>(spherical_pp());

    typedef bg::model::point
            <
                double, 2,
                bg::cs::geographic<bg::degree>
            > geo_point;

    test_all_pl_pl<geo_point>(vincenty_pp());
    test_all_pl_pl<geo_point>(thomas_pp());
    test_all_pl_pl<geo_point>(andoyer_pp());
    //test_all_pl_pl<geo_point>(karney_pp());

    test_all_pl_pl<geo_point>(andoyer_pp(stype(5000000,6000000)));
}
