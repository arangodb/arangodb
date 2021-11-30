// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2018-2020 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_geographic_linear_linear
#endif

#include <boost/type_traits/is_same.hpp>

#include <boost/test/included/unit_test.hpp>
#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include "test_distance_geo_common.hpp"
#include "test_empty_geometry.hpp"

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_segment_segment(Strategy const& strategy_ps)
{

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/segment distance tests" << std::endl;
#endif

    typedef bg::model::segment<Point> segment_type;

    typedef test_distance_of_geometries<segment_type, segment_type> tester;

    tester::apply("s-s-01",
                  "SEGMENT(0 0,1 1)",
                  "SEGMENT(2 0,3 0)",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(0 0,1 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-s-02",
                  "SEGMENT(2 1,3 1)",
                  "SEGMENT(2 0,3 0)",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(2 1,3 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-s-03",
                  "SEGMENT(2.5 1,3.5 1)",
                  "SEGMENT(2 0,3 0)",
                  ps_distance<Point>("POINT(2.5 0)",
                                     "SEGMENT(2.5 1,3.5 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-s-04",
                  "SEGMENT(2.5 1,3.5 1)",
                  "SEGMENT(2 2,3 2)",
                  ps_distance<Point>("POINT(3 2)",
                                     "SEGMENT(2.5 1,3.5 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-s-05",
                  "SEGMENT(2.5 2.1,3.5 1)",
                  "SEGMENT(2 2,3 2)",
                  0, strategy_ps, true, true, false);
    tester::apply("s-s-06",
                  "SEGMENT(2.5 2.1,3.5 1)",
                  "SEGMENT(2 2,3.5 1)",
                  0, strategy_ps, true, true, false);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_segment_linestring(Strategy const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/linestring distance tests" << std::endl;
#endif

    typedef bg::model::segment<Point> segment_type;
    typedef bg::model::linestring<Point> linestring_type;

    typedef test_distance_of_geometries<segment_type, linestring_type> tester;

    tester::apply("s-l-01",
                  "SEGMENT(0 0,1 1)",
                  "LINESTRING(2 0,3 0)",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(0 0,1 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-l-02",
                  "SEGMENT(0 0,1 1)",
                  "LINESTRING(2 0,3 0,2 2,0.5 0.7)",
                  ps_distance<Point>("POINT(1 1)",
                                     "SEGMENT(0.5 0.7,2 2)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-l-03",
                  "SEGMENT(0 0,2 2)",
                  "LINESTRING(2 0,3 0,2 2,0.5 0.7)",
                  0, strategy_ps, true, true, false);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_linestring_linestring(Strategy const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/linestring distance tests" << std::endl;
#endif

    typedef bg::model::linestring<Point> linestring_type;

    typedef test_distance_of_geometries
        <
            linestring_type, linestring_type
        > tester;

    tester::apply("l-l-01",
                  "LINESTRING(0 0,1 1)",
                  "LINESTRING(2 0,3 0)",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(0 0,1 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("l-l-02",
                  "LINESTRING(0 0,1 1,2 2)",
                  "LINESTRING(2 0,3 0,4 1)",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(1 1,2 2)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("l-l-03",
                  "LINESTRING(0 0,1 1,2 2)",
                  "LINESTRING(2 0)",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(1 1,2 2)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("l-l-04",
                  "LINESTRING(0 0,1 1,2 2)",
                  "LINESTRING(3 3,1 1)",
                  0, strategy_ps, true, true, false);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_segment_multilinestring(Strategy const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/multilinestring distance tests" << std::endl;
#endif

    typedef bg::model::segment<Point> segment_type;
    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;

    typedef test_distance_of_geometries
        <
            segment_type, multi_linestring_type
        > tester;

    tester::apply("s-ml-01",
                  "SEGMENT(0 0,1 1)",
                  "MULTILINESTRING((2 0,3 0)(2 5, 5 5, 2 -1))",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(0 0,1 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-ml-02",
                  "SEGMENT(0 0,1 1)",
                  "MULTILINESTRING((2 0,3 0))",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(0 0,1 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-ml-03",
                  "SEGMENT(0 0,2 0)",
                  "MULTILINESTRING((2 0,3 0)(2 5, 5 5, 2 -1))",
                  0, strategy_ps, true, true, false);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_linestring_multilinestring(Strategy const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/multilinestring distance tests" << std::endl;
#endif

    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;

    typedef test_distance_of_geometries
        <
            linestring_type, multi_linestring_type
        > tester;

    tester::apply("l-ml-01",
                  "LINESTRING(0 0,1 1,2 2,3 3,4 4,6 6)",
                  "MULTILINESTRING((2 0,3 0)(2 1, 5 5))",
                  ps_distance<Point>("POINT(5 5)",
                                     "SEGMENT(4 4,6 6)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("l-ml-02",
                  "LINESTRING(0 0,1 1,2 2,3 3,4 4,6 6)",
                  "MULTILINESTRING((2 0,3 0)(2 5, 5 5))",
                  0,
                  strategy_ps, true, true, false);

}

//===========================================================================

template <typename Point, typename Strategy>
void test_distance_multilinestring_multilinestring(Strategy const& strategy_ps)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/multilinestring distance tests" << std::endl;
#endif

    typedef bg::model::linestring<Point> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multi_linestring_type;

    typedef test_distance_of_geometries
        <
            multi_linestring_type, multi_linestring_type
        > tester;

    tester::apply("s-ml-01",
                  "MULTILINESTRING((0 0,1 1)(-1 0, -1 -1))",
                  "MULTILINESTRING((2 0,3 0)(2 5, 5 5))",
                  ps_distance<Point>("POINT(2 0)",
                                     "SEGMENT(0 0,1 1)", strategy_ps),
                  strategy_ps, true, true, false);
    tester::apply("s-ml-02",
                  "MULTILINESTRING((0 0,1 1)(-1 0, -1 -1)(5 0, 5 6))",
                  "MULTILINESTRING((2 0,3 0)(2 5, 5 5))",
                  0,
                  strategy_ps, true, true, false);
}

//===========================================================================
//===========================================================================
//===========================================================================

template <typename Point, typename Strategy>
void test_all_l_l(Strategy ps_strategy)
{
    test_distance_segment_segment<Point>(ps_strategy);
    test_distance_segment_linestring<Point>(ps_strategy);
    test_distance_linestring_linestring<Point>(ps_strategy);
    test_distance_segment_multilinestring<Point>(ps_strategy);
    test_distance_linestring_multilinestring<Point>(ps_strategy);
    test_distance_multilinestring_multilinestring<Point>(ps_strategy);

    test_more_empty_input_linear_linear<Point>(ps_strategy);
}

BOOST_AUTO_TEST_CASE( test_all_linear_linear )
{
    typedef bg::model::point
            <
                double, 2,
                bg::cs::spherical_equatorial<bg::degree>
            > sph_point;

    test_all_l_l<sph_point>(spherical_ps());

    typedef bg::model::point
            <
                double, 2,
                bg::cs::geographic<bg::degree>
            > geo_point;

    test_all_l_l<geo_point>(vincenty_ps());
    test_all_l_l<geo_point>(thomas_ps());
    test_all_l_l<geo_point>(andoyer_ps());
    //test_all_l_l<geo_point>(karney_ps());
}
