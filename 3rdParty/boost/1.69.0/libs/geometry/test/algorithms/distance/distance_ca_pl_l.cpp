// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_cartesian_pointlike_linear
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_common.hpp"
#include "test_empty_geometry.hpp"

typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::multi_point<point_type>            multi_point_type;
typedef bg::model::segment<point_type>                segment_type;
typedef bg::model::linestring<point_type>             linestring_type;
typedef bg::model::multi_linestring<linestring_type>  multi_linestring_type;

namespace services = bg::strategy::distance::services;
typedef bg::default_distance_result<point_type>::type return_type;

typedef bg::strategy::distance::pythagoras<> point_point_strategy;
typedef bg::strategy::distance::projected_point<> point_segment_strategy;


//===========================================================================


template <typename Strategy>
void test_distance_point_segment(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/segment distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, segment_type> tester;

    tester::apply("point(0 0)", "segment(2 0,3 0)", 2, 4, strategy);
    tester::apply("point(2.5 3)", "segment(2 0,3 0)", 3, 9, strategy);
    tester::apply("point(2 0)", "segment(2 0,3 0)", 0, 0, strategy);
    tester::apply("point(3 0)", "segment(2 0,3 0)", 0, 0, strategy);
    tester::apply("point(2.5 0)", "segment(2 0,3 0)", 0, 0, strategy);

    // distance is a NaN
    tester::apply("POINT(4.297374e+307 8.433875e+307)",
                  "SEGMENT(26 87,13 95)",
                  0, 0, strategy, false);
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

    tester::apply("point(0 0)", "linestring(2 0,3 0)", 2, 4, strategy);
    tester::apply("point(2.5 3)", "linestring(2 0,3 0)", 3, 9, strategy);
    tester::apply("point(2 0)", "linestring(2 0,3 0)", 0, 0, strategy);
    tester::apply("point(3 0)", "linestring(2 0,3 0)", 0, 0, strategy);
    tester::apply("point(2.5 0)", "linestring(2 0,3 0)", 0, 0, strategy);

    // linestring with a single point
    tester::apply("point(0 0)", "linestring(2 0)", 2, 4, strategy);

    // distance is a NaN
    tester::apply("POINT(4.297374e+307 8.433875e+307)",
                  "LINESTRING(26 87,13 95)",
                  0, 0, strategy, false);
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

    tester::apply("point(0 0)",
                  "multilinestring((-5 0,-3 0),(2 0,3 0))",
                  2, 4, strategy);
    tester::apply("point(2.5 3)",
                  "multilinestring((-5 0,-3 0),(2 0,3 0))",
                  3, 9, strategy);
    tester::apply("point(2 0)",
                  "multilinestring((-5 0,-3 0),(2 0,3 0))",
                  0, 0, strategy);
    tester::apply("point(3 0)",
                  "multilinestring((-5 0,-3 0),(2 0,3 0))",
                  0, 0, strategy);
    tester::apply("point(2.5 0)",
                  "multilinestring((-5 0,-3 0),(2 0,3 0))",
                  0, 0, strategy);
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((10 10,10 0),(0.0 -0.0,0.0 -0.0))",
                  0, 0, strategy);
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((10 10,10 0),(1 1,1 1))",
                  sqrt(2.0), 2, strategy);
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((10 10,10 0),(1 1,2 2))",
                  sqrt(2.0), 2, strategy);
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((10 10,10 0),(20 20,20 20))",
                  10, 100, strategy);

    // multilinestrings containing an empty linestring
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((),(10 0),(20 20,20 20))",
                  10, 100, strategy);
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((),(10 0),(),(20 20,20 20))",
                  10, 100, strategy);

    // multilinestrings containing a linestring with a single point
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((10 0),(20 20,20 20))",
                  10, 100, strategy);
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((20 20,20 20),(10 0))",
                  10, 100, strategy);

    // multilinestring with a single-point linestring and empty linestrings
    tester::apply("POINT(0 0)",
                  "MULTILINESTRING((),(20 20,20 20),(),(10 0))",
                  10, 100, strategy);
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

    tester::apply("linestring(2 0,0 2,100 100)",
                  "multipoint(0 0,1 0,0 1,1 1)",
                  0, 0, strategy);
    tester::apply("linestring(4 0,0 4,100 100)",
                  "multipoint(0 0,1 0,0 1,1 1)",
                  sqrt(2.0), 2, strategy);
    tester::apply("linestring(1 1,2 2,100 100)",
                  "multipoint(0 0,1 0,0 1,1 1)",
                  0, 0, strategy);
    tester::apply("linestring(3 3,4 4,100 100)",
                  "multipoint(0 0,1 0,0 1,1 1)",
                  sqrt(8.0), 8, strategy);

    // linestring with a single point
    tester::apply("linestring(1 8)",
                  "multipoint(0 0,3 0,4 -7,10 100)",
                  sqrt(65.0), 65, strategy);
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

    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((2 0,0 2),(2 2,3 3))",
                  0, 0, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((3 0,0 3),(4 4,5 5))",
                  0.5 * sqrt(2.0), 0.5, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((4 4,5 5),(1 1,2 2))",
                  0, 0, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((3 3,4 4),(4 4,5 5))",
                  sqrt(8.0), 8, strategy);

    // multilinestring with empty linestring
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((),(3 3,4 4),(4 4,5 5))",
                  sqrt(8.0), 8, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((3 3,4 4),(),(4 4,5 5))",
                  sqrt(8.0), 8, strategy);

    // multilinestrings with a single-point linestrings
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((3 3),(4 4,5 5))",
                  sqrt(8.0), 8, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((4 4,5 5),(3 3))",
                  sqrt(8.0), 8, strategy);

    // multilinestring with a single-point linestring and empty linestring
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multilinestring((4 4,5 5),(),(3 3))",
                  sqrt(8.0), 8, strategy);

    // 21890717 - assertion failure in distance(Pt, Box)
    {
        multi_point_type mpt;
        bg::read_wkt("multipoint(1 1,1 1,1 1,1 1,1 1,1 1,1 1,1 1,1 1)", mpt);
        multi_linestring_type mls;
        linestring_type ls;
        point_type pt(std::numeric_limits<double>::quiet_NaN(), 1.0);
        ls.push_back(pt);
        ls.push_back(pt);
        mls.push_back(ls);
        bg::distance(mpt, mls);
    }
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

    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "segment(2 0,0 2)",
                  0, 0, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "segment(4 0,0 4)",
                  sqrt(2.0), 2, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "segment(1 1,2 2)",
                  0, 0, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "segment(3 3,4 4)",
                  sqrt(8.0), 8, strategy);
    tester::apply("multipoint(4 4,5 5,2 2,3 3)",
                  "segment(0 0,1 1)",
                  sqrt(2.0), 2, strategy);
}

//===========================================================================
//===========================================================================
//===========================================================================

BOOST_AUTO_TEST_CASE( test_all_point_segment )
{
    test_distance_point_segment(point_point_strategy()); // back-compatibility
    test_distance_point_segment(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_point_linestring )
{
    test_distance_point_linestring(point_point_strategy()); // back-compatibility
    test_distance_point_linestring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_point_multilinestring )
{
    test_distance_point_multilinestring(point_point_strategy()); // back-compatibility
    test_distance_point_multilinestring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_linestring_multipoint )
{
    test_distance_linestring_multipoint(point_point_strategy()); // back-compatibility
    test_distance_linestring_multipoint(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_multilinestring )
{
    test_distance_multipoint_multilinestring(point_point_strategy()); // back-compatibility
    test_distance_multipoint_multilinestring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_segment )
{
    test_distance_multipoint_segment(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_empty_input_pointlike_linear )
{
    test_more_empty_input_pointlike_linear
        <
            point_type
        >(point_segment_strategy());
}
