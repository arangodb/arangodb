// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_pointlike_pointlike
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_common.hpp"


typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::multi_point<point_type>            multi_point_type;

namespace services = bg::strategy::distance::services;
typedef bg::default_distance_result<point_type>::type return_type;

typedef bg::strategy::distance::pythagoras<> point_point_strategy;

//===========================================================================

template <typename Strategy>
void test_distance_point_point(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/point distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, point_type> tester;

    tester::apply("point(1 1)",
                  "point(0 0)",
                  sqrt(2.0), 2, strategy);
    tester::apply("point(1 1)",
                  "point(1 1)",
                  0, 0, strategy);

    // distance overflows
    tester::apply("point(0 0)",
                  "point(4.297374e+307 8.433875e+307)",
                  0, 0, strategy, false);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_multipoint(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/multipoint distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, multi_point_type> tester;

    tester::apply("point(1 1)",
                  "multipoint(1 1,2 1,2 2,1 2)",
                  0, 0, strategy);
    tester::apply("point(1 1)",
                  "multipoint(2 2,2 3,3 2,3 3)",
                  sqrt(2.0), 2, strategy);
    tester::apply("point(3 0)",
                  "multipoint(2 2,2 4,4 2,4 4)",
                  sqrt(5.0), 5, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_multipoint(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/multipoint distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_point_type, multi_point_type
        > tester;

    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multipoint(1 1,2 1,2 2,1 2)",
                  0, 0, strategy);
    tester::apply("multipoint(0 0,1 0,0 1,1 1)",
                  "multipoint(2 2,2 3,3 2,3 3)",
                  sqrt(2.0), 2, strategy);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_more_empty_input_pointlike_pointlike(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::multi_point<Point> multipoint_empty;

    Point point = from_wkt<Point>("point(0 0)");

    // 1st geometry is empty
    test_empty_input(multipoint_empty, point, strategy);

    // 2nd geometry is empty
    test_empty_input(point, multipoint_empty, strategy);

    // both geometries are empty
    test_empty_input(multipoint_empty, multipoint_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}

//===========================================================================

BOOST_AUTO_TEST_CASE( test_all_point_point )
{
    test_distance_point_point(point_point_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_point_multipoint )
{
    test_distance_point_multipoint(point_point_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_multipoint )
{
    test_distance_multipoint_multipoint(point_point_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_empty_input_pointlike_pointlike )
{
    test_more_empty_input_pointlike_pointlike
        <
            point_type
        >(point_point_strategy());
}
