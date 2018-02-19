// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2016-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_geographic_pl_l
#endif

#include <boost/geometry/core/srs.hpp>
#include <boost/test/included/unit_test.hpp>

#include "test_distance_geo_common.hpp"

typedef bg::cs::geographic<bg::degree> cs_type;
typedef bg::model::point<double, 2, cs_type> point_type;
typedef bg::model::segment<point_type> segment_type;
typedef bg::model::multi_point<point_type> multi_point_type;
typedef bg::model::segment<point_type> segment_type;
typedef bg::model::linestring<point_type> linestring_type;
typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;

namespace services = bg::strategy::distance::services;
typedef bg::default_distance_result<point_type>::type return_type;

typedef bg::srs::spheroid<double> stype;

// Strategies for point-point distance

typedef bg::strategy::distance::andoyer<stype> andoyer_pp;
typedef bg::strategy::distance::thomas<stype> thomas_pp;
typedef bg::strategy::distance::vincenty<stype> vincenty_pp;

// Strategies for point-segment distance

typedef bg::strategy::distance::geographic_cross_track<bg::strategy::andoyer, stype, double>
        andoyer_strategy;

typedef bg::strategy::distance::geographic_cross_track<bg::strategy::thomas, stype, double>
        thomas_strategy;

typedef bg::strategy::distance::geographic_cross_track<bg::strategy::vincenty, stype, double>
        vincenty_strategy;

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
    return bg::distance(p1, p2, strategy);
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

//===========================================================================

template <typename Strategy_pp, typename Strategy_ps>
void test_distance_point_segment(Strategy_pp const& strategy_pp,
                                 Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/segment distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, segment_type> tester;

    tester::apply("p-s-01",
                  "POINT(0 0)",
                  "SEGMENT(2 0,3 0)",
                  pp_distance("POINT(0 0)", "POINT(2 0)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-02",
                  "POINT(2.5 3)",
                  "SEGMENT(2 0,3 0)",
                  pp_distance("POINT(2.5 3)", "POINT(2.49777 0)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-03",
                  "POINT(2 0)",
                  "SEGMENT(2 0,3 0)",
                  0,
                  strategy_ps);
    tester::apply("p-s-04",
                  "POINT(3 0)",
                  "SEGMENT(2 0,3 0)",
                  0,
                  strategy_ps);
    tester::apply("p-s-05",
                  "POINT(2.5 0)",
                  "SEGMENT(2 0,3 0)",
                  0,
                  strategy_ps);
    tester::apply("p-s-06",
                  "POINT(3.5 3)",
                  "SEGMENT(2 0,3 0)",
                  pp_distance("POINT(3 0)", "POINT(3.5 3)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-07",
                  "POINT(15 80)",
                  "SEGMENT(10 15,30 15)",
                  7204174.8264546748,
                  strategy_ps);
    tester::apply("p-s-08",
                  "POINT(15 10)",
                  "SEGMENT(10 15,30 15)",
                  571412.71247283253,
                  strategy_ps);
    tester::apply("p-s-09",
                  "POINT(5 10)",
                  "SEGMENT(10 15,30 15)",
                  pp_distance("POINT(5 10)", "POINT(10 15)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-10",
                  "POINT(35 10)",
                  "SEGMENT(10 15,30 15)",
                  pp_distance("POINT(35 10)", "POINT(30 15)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-11",
                  "POINT(5 10)",
                  "SEGMENT(30 15,10 15)",
                  pp_distance("POINT(5 10)", "POINT(10 15)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-12",
                  "POINT(35 10)",
                  "SEGMENT(30 15,10 15)",
                  pp_distance("POINT(35 10)", "POINT(30 15)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-right-up",
                  "POINT(3.5 3)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(3 2)", "POINT(3.5 3)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-left-up",
                  "POINT(1.5 3)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(2 2)", "POINT(1.5 3)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-up-1",
                  "POINT(2 3)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(2.0003 2)", "POINT(2 3)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-up-2",
                  "POINT(3 3)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(2.9997 2)", "POINT(3 3)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-right-down",
                  "POINT(3.5 1)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(3 2)", "POINT(3.5 1)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-left-down",
                  "POINT(1.5 1)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(2 2)", "POINT(1.5 1)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-down-1",
                  "POINT(2 1)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(2.0003 2)", "POINT(2 1)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-down-2",
                  "POINT(3 1)",
                  "SEGMENT(2 2,3 2)",
                  pp_distance("POINT(2.9997 2)", "POINT(3 1)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-south",
                  "POINT(3 -1)",
                  "SEGMENT(2 -2,3 -2)",
                  pp_distance("POINT(2.9997 -2)", "POINT(3 -1)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-antimeridian-1",
                  "POINT(3 1)",
                  "SEGMENT(220 2,3 2)",
                  pp_distance("POINT(3 2)", "POINT(3 1)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-antimeridian-2",
                  "POINT(220 1)",
                  "SEGMENT(220 2,3 2)",
                  pp_distance("POINT(220 2)", "POINT(220 1)", strategy_pp),
                  strategy_ps);

    // equator special case
    tester::apply("p-s-eq1",
                  "POINT(2.5 0)",
                  "SEGMENT(2 0,3 0)",
                  0,
                  strategy_ps);
    tester::apply("p-s-eq2",
                  "POINT(2.5 2)",
                  "SEGMENT(2 0,3 0)",
                  pp_distance("POINT(2.5 0)", "POINT(2.5 2)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-eq3",
                  "POINT(2 2)",
                  "SEGMENT(2 0,3 0)",
                  pp_distance("POINT(2 0)", "POINT(2 2)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-eq4",
                  "POINT(3 2)",
                  "SEGMENT(2 0,3 0)",
                  pp_distance("POINT(3 0)", "POINT(3 2)", strategy_pp),
                  strategy_ps);

    // meridian special case
    tester::apply("p-s-mer1",
                  "POINT(2.5 2)",
                  "SEGMENT(2 2,2 4)",
                  pp_distance("POINT(2.5 2)", "POINT(2 2)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-mer3",
                  "POINT(2.5 5)",
                  "SEGMENT(2 2,2 4)",
                  pp_distance("POINT(2.5 5)", "POINT(2 4)", strategy_pp),
                  strategy_ps);

    //degenerate segment
    tester::apply("p-s-deg",
                  "POINT(1 80)",
                  "SEGMENT(0 0,0 0)",
                  pp_distance("POINT(0 0)", "POINT(1 80)", strategy_pp),
                  strategy_ps);

    //point on segment
    tester::apply("p-s-deg",
                  "POINT(0 80)",
                  "SEGMENT(0 0,0 90)",
                  0,
                  strategy_ps);

    // very small distances to segment
    tester::apply("p-s-07",
                  "POINT(90 1e-3)",
                  "SEGMENT(0.5 0,175.5 0)",
                  pp_distance("POINT(90 0)", "POINT(90 1e-3)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-08",
                  "POINT(90 1e-4)",
                  "SEGMENT(0.5 0,175.5 0)",
                  pp_distance("POINT(90 0)", "POINT(90 1e-4)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-09",
                  "POINT(90 1e-5)",
                  "SEGMENT(0.5 0,175.5 0)",
                  pp_distance("POINT(90 0)", "POINT(90 1e-5)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-10",
                  "POINT(90 1e-6)",
                  "SEGMENT(0.5 0,175.5 0)",
                  pp_distance("POINT(90 0)", "POINT(90 1e-6)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-11",
                  "POINT(90 1e-7)",
                  "SEGMENT(0.5 0,175.5 0)",
                  pp_distance("POINT(90 0)", "POINT(90 1e-7)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-12",
                  "POINT(90 1e-8)",
                  "SEGMENT(0.5 0,175.5 0)",
                  pp_distance("POINT(90 0)", "POINT(90 1e-8)", strategy_pp),
                  strategy_ps);

    // very large distance to segment
    tester::apply("p-s-13",
                  "POINT(90 90)",
                  "SEGMENT(0.5 0,175.5 0)",
                  pp_distance("POINT(90 0)", "POINT(90 90)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-14",
                  "POINT(90 90)",
                  "SEGMENT(0.5 -89,175.5 -89)",
                  pp_distance("POINT(90 -89)", "POINT(90 90)", strategy_pp),
                  strategy_ps);
    // degenerate segment
    tester::apply("p-s-15",
                  "POINT(90 90)",
                  "SEGMENT(0.5 -90,175.5 -90)",
                  pp_distance("POINT(0.5 -90)", "POINT(90 90)", strategy_pp),
                  strategy_ps);
    tester::apply("p-s-16",
                  "POINT(90 90)",
                  "SEGMENT(0.5 -90,175.5 -90)",
                  pp_distance("POINT(90 -90)", "POINT(90 90)", strategy_pp),
                  strategy_ps);
    // equatorial segment
    tester::apply("p-s-17",
                  "POINT(90 90)",
                  "SEGMENT(0 0,175.5 0)",
                  pp_distance("POINT(90 90)", "POINT(0 0)", strategy_pp),
                  strategy_ps);
    // segment pass by pole
    tester::apply("p-s-18",
                  "POINT(90 90)",
                  "SEGMENT(0 0,180 0)",
                  0,
                  strategy_ps);

}

template <typename Strategy_pp, typename Strategy_ps>
void test_distance_point_segment_no_thomas(Strategy_pp const& strategy_pp,
                                           Strategy_ps const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/segment distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, segment_type> tester;

    // thomas strategy is failing for those test cases

    // meridian special case
    tester::apply("p-s-mer2",
                  "POINT(2.5 3)",
                  "SEGMENT(2 2,2 4)",
                  pp_distance("POINT(2.5 3)", "POINT(2 3.000114792872075)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-mer4",
                  "POINT(1 80)",
                  "SEGMENT(0 0,0 90)",
                  pp_distance("POINT(1 80)", "POINT(0 80.00149225834545)", strategy_pp),
                  strategy_ps);

    // Half meridian segment passing through pole
    tester::apply("p-s-19",
                  "POINT(90 89)",
                  "SEGMENT(0 0,180 0)",
                  pp_distance("POINT(90 89)", "POINT(90 90)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-20",
                  "POINT(80 89)",
                  "SEGMENT(0 0,180 0)",
                  pp_distance("POINT(80 89)", "POINT(0 89.82633489283377)", strategy_pp),
                  strategy_ps);

    tester::apply("p-s-20",
                  "POINT(80 89)",
                  "SEGMENT(0 -1,180 1)",
                  pp_distance("POINT(80 89)", "POINT(0 89.82633489283377)", strategy_pp),
                  strategy_ps);
}

//===========================================================================

template <typename Strategy_pp, typename Strategy_ps>
void test_distance_point_linestring(Strategy_pp const& strategy_pp,
                                    Strategy_ps const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/linestring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, linestring_type> tester;

    tester::apply("p-l-01",
                  "POINT(0 0)",
                  "LINESTRING(2 0,2 0)",
                  pp_distance("POINT(0 0)", "POINT(2 0)", strategy_pp),
                  strategy_ps);
    tester::apply("p-l-02",
                  "POINT(0 0)",
                  "LINESTRING(2 0,3 0)",
                  pp_distance("POINT(0 0)", "POINT(2 0)", strategy_pp),
                  strategy_ps);
    tester::apply("p-l-03",
                  "POINT(2.5 3)",
                  "LINESTRING(2 0,3 0)",
                  pp_distance("POINT(2.5 3)", "POINT(2.5 0)", strategy_pp),
                  strategy_ps);
    tester::apply("p-l-04",
                  "POINT(2 0)",
                  "LINESTRING(2 0,3 0)",
                  0,
                  strategy_ps);
    tester::apply("p-l-05",
                  "POINT(3 0)",
                  "LINESTRING(2 0,3 0)",
                  0,
                  strategy_ps);
    tester::apply("p-l-06",
                  "POINT(2.5 0)",
                  "LINESTRING(2 0,3 0)",
                  0,
                  strategy_ps);
    tester::apply("p-l-07",
                  "POINT(7.5 10)",
                  "LINESTRING(1 0,2 0,3 0,4 0,5 0,6 0,7 0,8 0,9 0)",
                  ps_distance("POINT(7.5 10)", "SEGMENT(7 0,8 0)", strategy_ps),
                  strategy_ps);
    tester::apply("p-l-08",
                  "POINT(7.5 10)",
                  "LINESTRING(1 1,2 1,3 1,4 1,5 1,6 1,7 1,20 2,21 2)",
                  ps_distance("POINT(7.5 10)", "SEGMENT(7 1,20 2)", strategy_ps),
                  strategy_ps);
}

void test_distance_point_linestring_strategies()
{
    typedef test_distance_of_geometries<point_type, linestring_type> tester;

    tester::apply("p-l-03",
                  "POINT(2.5 3)",
                  "LINESTRING(2 1,3 1)",
                  221147.24332788656,
                  vincenty_strategy());

    tester::apply("p-l-03",
                  "POINT(2.5 3)",
                  "LINESTRING(2 1,3 1)",
                  221147.36682199029,
                  thomas_strategy());

    tester::apply("p-l-03",
                  "POINT(2.5 3)",
                  "LINESTRING(2 1,3 1)",
                  221144.76527049288,
                  andoyer_strategy());
}

//===========================================================================

template <typename Strategy_pp, typename Strategy_ps>
void test_distance_point_multilinestring(Strategy_pp const& strategy_pp,
                                         Strategy_ps const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/multilinestring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            point_type, multi_linestring_type
        > tester;

    tester::apply("p-ml-01",
                  "POINT(0 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  pp_distance("POINT(0 0)", "POINT(2 0)", strategy_pp),
                  strategy_ps);
    tester::apply("p-ml-02",
                  "POINT(2.5 3)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  pp_distance("POINT(2.5 3)", "POINT(2.5 0)", strategy_pp),
                  strategy_ps);
    tester::apply("p-ml-03",
                  "POINT(2 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  0,
                  strategy_ps);
    tester::apply("p-ml-04",
                  "POINT(3 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  0,
                  strategy_ps);
    tester::apply("p-ml-05",
                  "POINT(2.5 0)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0))",
                  0,
                  strategy_ps);
    tester::apply("p-ml-06",
                  "POINT(7.5 10)",
                  "MULTILINESTRING((-5 0,-3 0),(2 0,3 0,4 0,5 0,6 0,20 1,21 1))",
                  ps_distance("POINT(7.5 10)", "SEGMENT(6 0,20 1)", strategy_ps),
                  strategy_ps);
    tester::apply("p-ml-07",
                  "POINT(-8 10)",
                  "MULTILINESTRING((-20 10,-19 11,-18 10,-6 0,-5 0,-3 0),(2 0,6 0,20 1,21 1))",
                  ps_distance("POINT(-8 10)", "SEGMENT(-6 0,-18 10)", strategy_ps),
                  strategy_ps);
}

//===========================================================================

template <typename Strategy_pp, typename Strategy_ps>
void test_distance_linestring_multipoint(Strategy_pp const& strategy_pp,
                                         Strategy_ps const& strategy_ps)
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
                  ps_distance("POINT(1 1)", "SEGMENT(2 0,0 2)", strategy_ps),
                  strategy_ps);

    tester::apply("l-mp-02",
                  "LINESTRING(4 0,0 4,100 80)",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  ps_distance("POINT(1 1)", "SEGMENT(0 4,4 0)", strategy_ps),
                  strategy_ps);
    tester::apply("l-mp-03",
                  "LINESTRING(1 1,2 2,100 80)",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  0,
                  strategy_ps);
    tester::apply("l-mp-04",
                  "LINESTRING(3 3,4 4,100 80)",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  pp_distance("POINT(1 1)", "POINT(3 3)", strategy_pp),
                  strategy_ps);
    tester::apply("l-mp-05",
                  "LINESTRING(0 0,10 0,10 10,0 10,0 0)",
                  "MULTIPOINT(1 -1,80 80,5 0,150 90)",
                  0,
                  strategy_ps);
}

//===========================================================================
template <typename Strategy_pp, typename Strategy_ps>
void test_distance_multipoint_multilinestring(Strategy_pp const& strategy_pp,
                                              Strategy_ps const& strategy_ps)
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
                  ps_distance("POINT(1 1)", "SEGMENT(2 0,0 2)", strategy_ps),
                  strategy_ps);
    tester::apply("mp-ml-02",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "MULTILINESTRING((3 0,0 3),(4 4,5 5))",
                  ps_distance("POINT(1 1)", "SEGMENT(3 0,0 3)", strategy_ps),
                  strategy_ps);
    tester::apply("mp-ml-03",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "MULTILINESTRING((4 4,5 5),(1 1,2 2))",
                  0,
                  strategy_ps);
    tester::apply("mp-ml-04",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "MULTILINESTRING((4 4,3 3),(4 4,5 5))",
                  pp_distance("POINT(1 1)", "POINT(3 3)", strategy_pp),
                  strategy_ps);
}

//===========================================================================

template <typename Strategy_pp, typename Strategy_ps>
void test_distance_multipoint_segment(Strategy_pp const& strategy_pp,
                                      Strategy_ps const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/segment distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<multi_point_type, segment_type> tester;

    tester::apply("mp-s-01",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(2 0,0 2)",
                  ps_distance("POINT(1 1)", "SEGMENT(2 0,0 2)", strategy_ps),
                  strategy_ps);
    tester::apply("mp-s-02",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(0 -3,1 -10)",
                  pp_distance("POINT(0 0)", "POINT(0 -3)", strategy_pp),
                  strategy_ps);
    tester::apply("mp-s-03",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(1 1,2 2)",
                  0,
                  strategy_ps);
    tester::apply("mp-s-04",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(3 3,4 4)",
                  pp_distance("POINT(1 1)", "POINT(3 3)", strategy_pp),
                  strategy_ps);
    tester::apply("mp-s-05",
                  "MULTIPOINT(0 0,1 0,0 1,1 1)",
                  "SEGMENT(0.5 -3,1 -10)",
                  pp_distance("POINT(1 0)", "POINT(0.5 -3)", strategy_pp),
                  strategy_ps);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_empty_input_pointlike_linear(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::linestring<Point> line_empty;
    bg::model::multi_point<Point> multipoint_empty;
    bg::model::multi_linestring<bg::model::linestring<Point> > multiline_empty;

    Point point = from_wkt<Point>("POINT(0 0)");
    bg::model::linestring<Point> line =
        from_wkt<bg::model::linestring<Point> >("LINESTRING(0 0,1 1)");

    // 1st geometry is empty
    //test_empty_input(multipoint_empty, line, strategy);

    // 2nd geometry is empty
    test_empty_input(point, line_empty, strategy);
    test_empty_input(point, multiline_empty, strategy);

    // both geometries are empty
    //test_empty_input(multipoint_empty, line_empty, strategy);
    //test_empty_input(multipoint_empty, multiline_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}

//===========================================================================
//===========================================================================
//===========================================================================

BOOST_AUTO_TEST_CASE( test_all_point_segment )
{
    //TODO: Operations with multipoints need geographic pt-box strategy
    //before activating

    test_distance_point_segment(vincenty_pp(), vincenty_strategy());
    test_distance_point_segment(thomas_pp(), thomas_strategy());
    test_distance_point_segment(andoyer_pp(), andoyer_strategy());

    test_distance_point_segment_no_thomas(vincenty_pp(), vincenty_strategy());
    //test_distance_point_segment_no_thomas(thomas_pp(), thomas_strategy());
    test_distance_point_segment_no_thomas(andoyer_pp(), andoyer_strategy());

    test_distance_point_linestring(vincenty_pp(), vincenty_strategy());
    test_distance_point_linestring(thomas_pp(), thomas_strategy());
    test_distance_point_linestring(andoyer_pp(), andoyer_strategy());
    test_distance_point_linestring_strategies();

    test_distance_point_multilinestring(vincenty_pp(), vincenty_strategy());
    test_distance_point_multilinestring(thomas_pp(), thomas_strategy());
    test_distance_point_multilinestring(andoyer_pp(), andoyer_strategy());

    //    test_distance_linestring_multipoint(vincenty_pp(), vincenty_strategy());
    //    test_distance_linestring_multipoint(thomas_pp(), thomas_strategy());
    //    test_distance_linestring_multipoint(andoyer_pp(), andoyer_strategy());

    //    test_distance_multipoint_multilinestring(vincenty_pp(), vincenty_strategy());
    //    test_distance_multipoint_multilinestring(thomas_pp(), thomas_strategy());
    //    test_distance_multipoint_multilinestring(andoyer_pp(), andoyer_strategy());

    //    test_distance_multipoint_segment(vincenty_pp(), vincenty_strategy());
    //    test_distance_multipoint_segment(thomas_pp(), thomas_strategy());
    //    test_distance_multipoint_segment(andoyer_pp(), andoyer_strategy());

    test_empty_input_pointlike_linear<point_type>(vincenty_strategy());
}
