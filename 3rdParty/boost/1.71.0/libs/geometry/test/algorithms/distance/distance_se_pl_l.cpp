// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>
#define BOOST_GEOMETRY_TEST_DEBUG

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_spherical_equatorial_pointlike_linear
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_se_common.hpp"
#include "test_empty_geometry.hpp"

typedef bg::cs::spherical_equatorial<bg::degree> cs_type;
typedef bg::model::point<double, 2, cs_type> point_type;
typedef bg::model::segment<point_type> segment_type;
typedef bg::model::multi_point<point_type> multi_point_type;
typedef bg::model::segment<point_type> segment_type;
typedef bg::model::linestring<point_type> linestring_type;
typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;

namespace services = bg::strategy::distance::services;
typedef bg::default_distance_result<point_type>::type return_type;

typedef bg::strategy::distance::haversine<double> point_point_strategy;
typedef bg::strategy::distance::cross_track<> point_segment_strategy;


//===========================================================================

template <typename Strategy>
inline bg::default_distance_result<point_type>::type
pp_distance(std::string const& wkt1,
            std::string const& wkt2,
            Strategy const& strategy)
{
    point_type p1, p2;
    bg::read_wkt(wkt1, p1);
    bg::read_wkt(wkt2, p2);
    return bg::distance(p1, p2) * strategy.radius();
}

template <typename Strategy>
inline bg::default_comparable_distance_result<point_type>::type
pp_comparable_distance(std::string const& wkt1,
                       std::string const& wkt2,
                       Strategy const&)
{
    point_type p1, p2;
    bg::read_wkt(wkt1, p1);
    bg::read_wkt(wkt2, p2);
    return bg::comparable_distance(p1, p2);
}

template <typename Strategy>
inline bg::default_distance_result<point_type>::type
ps_distance(std::string const& wkt1,
            std::string const& wkt2,
            Strategy const& strategy)
{
    point_type p;
    segment_type s;
    bg::read_wkt(wkt1, p);
    bg::read_wkt(wkt2, s);
    return bg::distance(p, s, strategy);
}

template <typename Strategy>
inline bg::default_comparable_distance_result<point_type>::type
ps_comparable_distance(std::string const& wkt1,
                       std::string const& wkt2,
                       Strategy const& strategy)
{
    point_type p;
    segment_type s;
    bg::read_wkt(wkt1, p);
    bg::read_wkt(wkt2, s);
    return bg::comparable_distance(p, s, strategy);
}

template <typename Strategy, typename T>
T to_comparable(Strategy const& strategy, T const& distance)
{
    namespace services = bg::strategy::distance::services;

    typedef typename services::comparable_type
        <
            Strategy
        >::type comparable_strategy;

    typedef typename services::result_from_distance
        <
            comparable_strategy,
            point_type,
            bg::point_type<segment_type>::type
        > get_comparable_distance;

    comparable_strategy cstrategy = services::get_comparable
        <
            Strategy
        >::apply(strategy);

    return get_comparable_distance::apply(cstrategy, distance);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_segment(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/segment distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, segment_type> tester;

    double const d2r = bg::math::d2r<double>();

    tester::apply("p-s-01",
                  "POINT(0 0)",
                  "SEGMENT(2 0,3 0)",
                  2.0 * d2r * strategy.radius(),
                  to_comparable(strategy, 2.0 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-s-02",
                  "POINT(2.5 3)",
                  "SEGMENT(2 0,3 0)",
                  3.0 * d2r * strategy.radius(),
                  to_comparable(strategy, 3.0 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-s-03",
                  "POINT(2 0)",
                  "SEGMENT(2 0,3 0)",
                  0,
                  strategy);
    tester::apply("p-s-04",
                  "POINT(3 0)",
                  "SEGMENT(2 0,3 0)",
                  0,
                  strategy);
    tester::apply("p-s-05",
                  "POINT(2.5 0)",
                  "SEGMENT(2 0,3 0)",
                  0,
                  strategy);
    tester::apply("p-s-06",
                  "POINT(3.5 3)",
                  "SEGMENT(2 0,3 0)",
                  pp_distance("POINT(3 0)", "POINT(3.5 3)", strategy),
                  pp_comparable_distance("POINT(3 0)",
                                         "POINT(3.5 3)",
                                         strategy),
                  strategy);
    tester::apply("p-s-07",
                  "POINT(0 0)",
                  "SEGMENT(0 10,10 10)",
                  ps_distance("POINT(0 0)", "SEGMENT(10 10,0 10)", strategy),
                  pp_comparable_distance("POINT(0 0)",
                                         "POINT(0 10)",
                                         strategy),
                  strategy);
    // very small distances to segment
    tester::apply("p-s-07",
                  "POINT(90 1e-3)",
                  "SEGMENT(0.5 0,175.5 0)",
                  1e-3 * d2r * strategy.radius(),
                  to_comparable(strategy, 1e-3 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-s-08",
                  "POINT(90 1e-4)",
                  "SEGMENT(0.5 0,175.5 0)",
                  1e-4 * d2r * strategy.radius(),
                  to_comparable(strategy, 1e-4 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-s-09",
                  "POINT(90 1e-5)",
                  "SEGMENT(0.5 0,175.5 0)",
                  1e-5 * d2r * strategy.radius(),
                  to_comparable(strategy, 1e-5 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-s-10",
                  "POINT(90 1e-6)",
                  "SEGMENT(0.5 0,175.5 0)",
                  1e-6 * d2r * strategy.radius(),
                  to_comparable(strategy, 1e-6 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-s-11",
                  "POINT(90 1e-7)",
                  "SEGMENT(0.5 0,175.5 0)",
                  1e-7 * d2r * strategy.radius(),
                  to_comparable(strategy, 1e-7 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-s-12",
                  "POINT(90 1e-8)",
                  "SEGMENT(0.5 0,175.5 0)",
                  1e-8 * d2r * strategy.radius(),
                  to_comparable(strategy, 1e-8 * d2r * strategy.radius()),
                  strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_linestring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/linestring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, linestring_type> tester;

    double const r = strategy.radius();
    double const d2r = bg::math::d2r<double>();

    tester::apply("p-l-01",
                  "POINT(0 0)",
                  "LINESTRING(2 0,2 0)",
                  2.0 * d2r * r,
                  to_comparable(strategy, 2.0 * d2r * r),
                  strategy);
    tester::apply("p-l-02",
                  "POINT(0 0)",
                  "LINESTRING(2 0,3 0)",
                  2.0 * d2r * r,
                  to_comparable(strategy, 2.0 * d2r * r),
                  strategy);
    tester::apply("p-l-03",
                  "POINT(2.5 3)",
                  "LINESTRING(2 0,3 0)",
                  3.0 * d2r * r,
                  to_comparable(strategy, 3.0 * d2r * r),
                  strategy);
    tester::apply("p-l-04",
                  "POINT(2 0)",
                  "LINESTRING(2 0,3 0)",
                  0,
                  strategy);
    tester::apply("p-l-05",
                  "POINT(3 0)",
                  "LINESTRING(2 0,3 0)",
                  0,
                  strategy);
    tester::apply("p-l-06",
                  "POINT(2.5 0)",
                  "LINESTRING(2 0,3 0)",
                  0,
                  strategy);
    tester::apply("p-l-07",
                  "POINT(7.5 10)",
                  "LINESTRING(1 0,2 0,3 0,4 0,5 0,6 0,7 0,8 0,9 0)",
                  10.0 * d2r * r,
                  to_comparable(strategy, 10.0 * d2r * r),
                  strategy);
    tester::apply("p-l-08",
                  "POINT(7.5 10)",
                  "LINESTRING(1 1,2 1,3 1,4 1,5 1,6 1,7 1,20 2,21 2)",
                  ps_distance("POINT(7.5 10)", "SEGMENT(7 1,20 2)", strategy),
                  ps_comparable_distance("POINT(7.5 10)",
                                         "SEGMENT(7 1,20 2)",
                                         strategy),
                  strategy);

    // https://svn.boost.org/trac/boost/ticket/11982
    tester::apply("p-l-09",
                  "POINT(10.4 63.43)",
                  "LINESTRING(10.733557 59.911923, 10.521812 59.887214)",
                  0.06146397739758279 * r,
                  0.000944156107132969,
                  strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_multilinestring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/multilinestring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            point_type, multi_linestring_type
        > tester;

    double const d2r = bg::math::d2r<double>();

    tester::apply("p-ml-01",
                  "POINT(0 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  2.0 * d2r * strategy.radius(),
                  to_comparable(strategy, 2.0 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-ml-02",
                  "POINT(2.5 3)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  3.0 * d2r * strategy.radius(),
                  to_comparable(strategy, 3.0 * d2r * strategy.radius()),
                  strategy);
    tester::apply("p-ml-03",
                  "POINT(2 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  0,
                  strategy);
    tester::apply("p-ml-04",
                  "POINT(3 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  0,
                  strategy);
    tester::apply("p-ml-05",
                  "POINT(2.5 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  0,
                  strategy);
    tester::apply("p-ml-06",
                  "POINT(7.5 10)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0,4 0,5 0,6 0,20 1,21 1))",
                  ps_distance("POINT(7.5 10)", "SEGMENT(6 0,20 1)", strategy),
                  ps_comparable_distance("POINT(7.5 10)",
                                         "SEGMENT(6 0,20 1)",
                                         strategy),
                  strategy);
    tester::apply("p-ml-07",
                  "POINT(-8 10)",
                  "MULTILINESTRING((-20 10,-19 11,-18 10,-6 0,-5 0,-3 0),(2 0,6 0,20 1,21 1))",
                  ps_distance("POINT(-8 10)", "SEGMENT(-6 0,-18 10)", strategy),
                  ps_comparable_distance("POINT(-8 10)",
                                         "SEGMENT(-6 0,-18 10)",
                                         strategy),
                  strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_linestring_multipoint(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/multipoint distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            linestring_type, multi_point_type
        > tester;

    tester::apply("l-mp-01",
                  "LINESTRING(2 0,0 2,100 80)",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  ps_distance("POINT(1 1)", "SEGMENT(2 0,0 2)", strategy),
                  ps_comparable_distance("POINT(1 1)",
                                         "SEGMENT(2 0,0 2)",
                                         strategy),
                  strategy);
    tester::apply("l-mp-02",
                  "LINESTRING(4 0,0 4,100 80)",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  ps_distance("POINT(1 1)", "SEGMENT(0 4,4 0)", strategy),
                  ps_comparable_distance("POINT(1 1)",
                                         "SEGMENT(0 4,4 0)",
                                         strategy),
                  strategy);
    tester::apply("l-mp-03",
                  "LINESTRING(1 1,2 2,100 80)",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  0,
                  strategy);
    tester::apply("l-mp-04",
                  "LINESTRING(3 3,4 4,100 80)",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  pp_distance("POINT(1 1)", "POINT(3 3)", strategy),
                  pp_comparable_distance("POINT(1 1)", "POINT(3 3)", strategy),
                  strategy);
    tester::apply("l-mp-05",
                  "LINESTRING(0 0,10 0,10 10,0 10,0 0)",
                  "MULTIPOINT(1 -1,80 80,5 0,150 90)",
                  0,
                  strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_multilinestring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/multilinestring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_point_type, multi_linestring_type
        > tester;

    tester::apply("mp-ml-01",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "MULTILINESTRING((2 0,0 2),(2 2,3 3))",
                  ps_distance("POINT(1 1)", "SEGMENT(2 0,0 2)", strategy),
                  ps_comparable_distance("POINT(1 1)",
                                         "SEGMENT(2 0,0 2)",
                                         strategy),
                  strategy);
    tester::apply("mp-ml-02",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "MULTILINESTRING((3 0,0 3),(4 4,5 5))",
                  ps_distance("POINT(1 1)", "SEGMENT(3 0,0 3)", strategy),
                  ps_comparable_distance("POINT(1 1)",
                                         "SEGMENT(3 0,0 3)",
                                         strategy),
                  strategy);
    tester::apply("mp-ml-03",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "MULTILINESTRING((4 4,5 5),(1 1,2 2))",
                  0,
                  strategy);
    tester::apply("mp-ml-04",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "MULTILINESTRING((4 4,3 3),(4 4,5 5))",
                  pp_distance("POINT(1 1)", "POINT(3 3)", strategy),
                  pp_comparable_distance("POINT(1 1)", "POINT(3 3)", strategy),
                  strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_segment(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/segment distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<multi_point_type, segment_type> tester;

    double d2r = bg::math::d2r<double>();

    tester::apply("mp-s-01",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(2 0,0 2)",
                  ps_distance("POINT(1 1)", "SEGMENT(2 0,0 2)", strategy),
                  ps_comparable_distance("POINT(1 1)",
                                         "SEGMENT(2 0,0 2)",
                                         strategy),
                  strategy);
    tester::apply("mp-s-02",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(0 -3,1 -10)",
                  3.0 * d2r * strategy.radius(),
                  to_comparable(strategy, 3.0 * d2r * strategy.radius()),
                  strategy);
    tester::apply("mp-s-03",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(1 1,2 2)",
                  0,
                  strategy);
    tester::apply("mp-s-04",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(3 3,4 4)",
                  pp_distance("POINT(1 1)", "POINT(3 3)", strategy),
                  pp_comparable_distance("POINT(1 1)", "POINT(3 3)", strategy),
                  strategy);
    tester::apply("mp-s-05",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(0.5 -3,1 -10)",
                  pp_distance("POINT(1 0)", "POINT(0.5 -3)", strategy),
                  pp_comparable_distance("POINT(1 0)",
                                         "POINT(0.5 -3)",
                                         strategy),
                  strategy);
}

//===========================================================================
//===========================================================================
//===========================================================================

BOOST_AUTO_TEST_CASE( test_all_pointlike_linear )
{
    test_distance_point_segment(point_segment_strategy());
    test_distance_point_segment(point_segment_strategy(earth_radius_km));
    test_distance_point_segment(point_segment_strategy(earth_radius_miles));
}

BOOST_AUTO_TEST_CASE( test_all_point_linestring )
{
    test_distance_point_linestring(point_segment_strategy());
    test_distance_point_linestring(point_segment_strategy(earth_radius_km));
    test_distance_point_linestring(point_segment_strategy(earth_radius_miles));
}

BOOST_AUTO_TEST_CASE( test_all_point_multilinestring )
{
    test_distance_point_multilinestring(point_segment_strategy());
    test_distance_point_multilinestring(point_segment_strategy(earth_radius_km));
    test_distance_point_multilinestring(point_segment_strategy(earth_radius_miles));
}

BOOST_AUTO_TEST_CASE( test_all_linestring_multipoint )
{
    test_distance_linestring_multipoint(point_segment_strategy());
    test_distance_linestring_multipoint(point_segment_strategy(earth_radius_km));
    test_distance_linestring_multipoint(point_segment_strategy(earth_radius_miles));
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_multilinestring )
{
    test_distance_multipoint_multilinestring(point_segment_strategy());
    test_distance_multipoint_multilinestring(point_segment_strategy(earth_radius_km));
    test_distance_multipoint_multilinestring(point_segment_strategy(earth_radius_miles));
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_segment )
{
    test_distance_multipoint_segment(point_segment_strategy());
    test_distance_multipoint_segment(point_segment_strategy(earth_radius_km));
    test_distance_multipoint_segment(point_segment_strategy(earth_radius_miles));
}

BOOST_AUTO_TEST_CASE( test_all_empty_input_pointlike_linear )
{
    test_more_empty_input_pointlike_linear
        <
            point_type
        >(point_segment_strategy());

    test_more_empty_input_pointlike_linear
        <
            point_type
        >(point_segment_strategy(earth_radius_km));

    test_more_empty_input_pointlike_linear
        <
            point_type
        >(point_segment_strategy(earth_radius_miles));
}
