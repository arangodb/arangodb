// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_linear_areal
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_common.hpp"


typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::point<int,2,bg::cs::cartesian>     int_point_type;
typedef bg::model::segment<point_type>                segment_type;
typedef bg::model::segment<int_point_type>            int_segment_type;
typedef bg::model::linestring<point_type>             linestring_type;
typedef bg::model::multi_linestring<linestring_type>  multi_linestring_type;
typedef bg::model::polygon<point_type, false>         polygon_type;
typedef bg::model::polygon<point_type, false, false>  open_polygon_type;
typedef bg::model::multi_polygon<polygon_type>        multi_polygon_type;
typedef bg::model::multi_polygon<open_polygon_type>   open_multipolygon_type;
typedef bg::model::ring<point_type, false>            ring_type;
typedef bg::model::box<point_type>                    box_type;
typedef bg::model::box<int_point_type>                int_box_type;

namespace services = bg::strategy::distance::services;
typedef bg::default_distance_result<point_type>::type return_type;

typedef bg::strategy::distance::pythagoras<> point_point_strategy;
typedef bg::strategy::distance::projected_point<> point_segment_strategy;

//===========================================================================

template <typename Strategy>
void test_distance_segment_polygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/polygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<segment_type, polygon_type> tester;

    tester::apply("segment(-1 20,1 20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);

    tester::apply("segment(1 20,2 40)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);

    tester::apply("segment(-1 20,-1 5)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("segment(-1 20,-1 -20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("segment(0 0,1 1)",
                  "polygon((2 2))",
                  sqrt(2.0), 2, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_linestring_polygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/polygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<linestring_type, polygon_type> tester;

    tester::apply("linestring(-1 20,1 20,1 30)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);
  
    tester::apply("linestring(-5 1,-2 1)",
                  "polygon((0 0,10 0,10 10,0 10,0 0))",
                  2, 4, strategy);

    tester::apply("linestring(-1 20,1 20,1 5)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("linestring(-1 20,1 20,1 -20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("linestring(-2 1)",
                  "polygon((0 0,10 0,10 10,0 10,0 0))",
                  2, 4, strategy);

    tester::apply("linestring(-5 1,-2 1)",
                  "polygon((0 0))",
                  sqrt(5.0), 5, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_linestring_open_polygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/open polygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            linestring_type, open_polygon_type
        > tester;

    tester::apply("linestring(-5 1,-2 1)",
                  "polygon((0 0,10 0,10 10,0 10))",
                  2, 4, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multilinestring_polygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/polygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_linestring_type, polygon_type
        > tester;

    tester::apply("multilinestring((-100 -100,-90 -90),(-1 20,1 20,1 30))",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);
  
    tester::apply("multilinestring((-1 20,1 20,1 30),(-1 20,1 20,1 5))",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("multilinestring((-1 20,1 20,1 30),(-1 20,1 20,1 -20))",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("multilinestring((-100 -100,-90 -90),(1 20))",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);

    tester::apply("multilinestring((-100 -100,-90 -90),(-1 20,1 20,1 30))",
                  "polygon((-110 -110))",
                  sqrt(200.0), 200, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_segment_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            segment_type, multi_polygon_type
        > tester;

    tester::apply("segment(-1 20,1 20)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((0 22,-1 30, 2 40,0 22)))",
                  2, 4, strategy);

    tester::apply("segment(12 0,14 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  2, 4, strategy);

    tester::apply("segment(12 0,20.5 0.5)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);

    tester::apply("segment(12 0,50 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_linestring_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            linestring_type, multi_polygon_type
        > tester;

    tester::apply("linestring(-1 20,1 20)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((0 22,-1 30, 2 40,0 22)))",
                  2, 4, strategy);
    
    tester::apply("linestring(12 0,14 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  2, 4, strategy);

    tester::apply("linestring(12 0,20.5 0.5)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);

    tester::apply("linestring(12 0,50 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_linestring_open_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/open multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            linestring_type, open_multipolygon_type
        > tester;

    tester::apply("linestring(-5 1,-2 1)",
                  "multipolygon(((0 0,10 0,10 10,0 10)))",
                  2, 4, strategy);

    tester::apply("linestring(-5 1,-3 1)",
                  "multipolygon(((20 20,21 20,21 21,20 21)),((0 0,10 0,10 10,0 10)))",
                  3, 9, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multilinestring_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_linestring_type, multi_polygon_type
        > tester;

    tester::apply("multilinestring((12 0,14 0),(19 0,19.9 -1))",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10)))",
                  0.1, 0.01, strategy);

    tester::apply("multilinestring((19 0,19.9 -1),(12 0,20.5 0.5))",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_segment_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "segment/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<segment_type, ring_type> tester;

    tester::apply("segment(-1 20,1 20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);

    tester::apply("segment(1 20,2 40)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);

    tester::apply("segment(-1 20,-1 5)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("segment(-1 20,-1 -20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_linestring_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<linestring_type, ring_type> tester;

    tester::apply("linestring(-1 20,1 20,1 30)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);
  
    tester::apply("linestring(-1 20,1 20,1 5)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("linestring(-1 20,1 20,1 -20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multilinestring_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_linestring_type, ring_type
        > tester;

    tester::apply("multilinestring((-100 -100,-90 -90),(-1 20,1 20,1 30))",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);
  
    tester::apply("multilinestring((-1 20,1 20,1 30),(-1 20,1 20,1 5))",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("multilinestring((-1 20,1 20,1 30),(-1 20,1 20,1 -20))",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_segment_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "2D segment/box distance tests" << std::endl;
#endif
    typedef int_box_type B;
    typedef segment_type S;
    typedef int_segment_type IS;

    typedef test_distance_of_geometries<B, S> tester;
    typedef test_distance_of_geometries<B, IS> itester;

    // 1st example by Adam Wulkiewicz
    tester::apply("BOX(5 51,42 96)",
                  "SEGMENT(6.6799994 95.260002,35.119999 56.340004)",
                  0, 0, strategy);

    // 2nd example by Adam Wulkiewicz
    tester::apply("BOX(51 55,94 100)",
                  "SEGMENT(92.439995 50.130001,59.959999 80.870003)",
                  0, 0, strategy);

    // segments that intersect the box
    tester::apply("box(0 0,1 1)",
                  "segment(-1 0.5,0.5 0.75)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 0.5,1.5 0.75)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -1,0.5 2)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 1,1.5 0.75)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(2 0,0 2)",
                  0, 0, strategy);
    
    // segment that has closest point on box boundary
    tester::apply("box(0 0,1 1)",
                  "segment(4 0.5,5 0.75)",
                  3, 9, strategy);

    // segment that has closest point on box corner
    tester::apply("box(0 0,1 1)",
                  "segment(4 0,0 4)",
                  sqrt(2.0), 2, strategy);
    itester::apply("box(0 0,1 1)",
                   "segment(-4 0,0 -4)",
                   sqrt(8.0), 8, strategy);
    itester::apply("box(0 0,1 1)",
                   "segment(-8 4,4 -8)",
                   sqrt(8.0), 8, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-4 0,0 4)",
                  1.5 * sqrt(2.0), 4.5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-4 0,1 5)",
                  1.5 * sqrt(2.0), 4.5, strategy);
    itester::apply("box(0 0,1 1)",
                   "segment(0 -2,3 1)",
                   0.5 * sqrt(2.0), 0.5, strategy);
    itester::apply("box(0 0,1 1)",
                   "segment(0 -2,2 2)",
                   0, 0, strategy);

    // horizontal segments
    itester::apply("box(0 0,1 1)",
                   "segment(-2 -1,-1 -1)",
                   sqrt(2.0), 2, strategy);
    itester::apply("box(0 0,1 1)",
                   "segment(-1 -1,0 -1)",
                   1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-0.5 -1,0.5 -1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -1,0.75 -1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -1,1.25 -1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 -1,2 -1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(2 -1,3 -1)",
                  sqrt(2.0), 2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -1,2 -1)",
                  1, 1, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(-2 0,-1 0)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 0,0 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-0.5 0,0.5 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 0,0.75 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 0,1.25 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 0,2 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(2 0,3 0)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 0,2 0)",
                  0, 0, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(-2 0.5,-1 0.5)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 0.5,0 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-0.5 0.5,0.5 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 0.5,0.75 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 0.5,1.25 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 0.5,2 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(2 0.5,3 0.5)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 0.5,2 0.5)",
                  0, 0, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(-2 1,-1 1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 1,0 1)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-0.5 1,0.5 1)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 1,0.75 1)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 1,1.25 1)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 1,2 1)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(2 1,3 1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 1,2 1)",
                  0, 0, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(-2 3,-1 3)",
                  sqrt(5.0), 5, strategy);
    itester::apply("box(0 0,1 1)",
                   "segment(-1 3,0 3)",
                   2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-0.5 3,0.5 3)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 3,0.75 3)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 3,1.25 3)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 3,2 3)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(2 3,3 3)",
                  sqrt(5.0), 5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 3,2 3)",
                  2, 4, strategy);

    // vertical segments
    tester::apply("box(0 0,1 1)",
                  "segment(-1 -2,-1 -1)",
                  sqrt(2.0), 2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 -1,-1 0)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 -0.5,-1 0.5)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 0.5,-1 0.75)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 0.5,-1 1.25)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 1,-1 2)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 2,-1 3)",
                  sqrt(2.0), 2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 -2,-1 2)",
                  1, 1, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(0 -2,0 -1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 -1,0 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 -0.5,0 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 0.5,0 0.75)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 0.5,0 1.25)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 1,0 2)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 2,0 3)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 -2,0 2)",
                  0, 0, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -2,0.5 -1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -1,0.5 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -0.5,0.5 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 0.5,0.5 0.75)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 0.5,0.5 1.25)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 1,0.5 2)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 2,0.5 3)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -2,0.5 2)",
                  0, 0, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(1 -2,1 -1)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 -1,1 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 -0.5,1 0.5)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 0.5,1 0.75)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 0.5,1 1.25)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 1,1 2)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 2,1 3)",
                  1, 1, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 -2,1 2)",
                  0, 0, strategy);

    tester::apply("box(0 0,1 1)",
                  "segment(3 -2,3 -1)",
                  sqrt(5.0), 5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 -1,3 0)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 -0.5,3 0.5)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 0.5,3 0.75)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 0.5,3 1.25)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 1,3 2)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 2,3 3)",
                  sqrt(5.0), 5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 -2,3 2)",
                  2, 4, strategy);

    // positive slope
    itester::apply("box(0 0,1 1)",
                   "segment(-2 -2,-1 -1)",
                   sqrt(2.0), 2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,0 -0.5)",
                  0.5, 0.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,0.5 -0.5)",
                  0.5, 0.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,1 -0.5)",
                  0.5, 0.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,2 0)",
                  sqrt(0.2), 0.2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,4 1)",
                  sqrt(0.2), 0.2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,-1.5 0)",
                  1.5, 2.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,-1.5 0.5)",
                  1.5, 2.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,-1.5 1)",
                  1.5, 2.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,0 2)",
                  sqrt(0.2), 0.2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,1 4)",
                  sqrt(0.2), 0.2, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,4 2)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,2 4)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,4 3)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,3 4)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,3 3)",
                  0, 0, strategy);

    // negative slope
    tester::apply("box(0 0,1 1)",
                  "segment(-2 -2,-1 -3)",
                  sqrt(8.0), 8, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-3 -1,0 -4)",
                  sqrt(8.0), 8, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 0.75,-1.5 0.5)",
                  1.5, 2.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 1.5,-1.5 0.5)",
                  1.5, 2.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 2,0.75 1.5)",
                  0.5, 0.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 2,0.75 1.5)",
                  0.5, 0.25, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 2,2 0)",
                  0, 0, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0 3,3 0)",
                  sqrt(0.5), 0.5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 4,4 -1)",
                  sqrt(0.5), 0.5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 4,0 3)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-2 5,-1 4)",
                  sqrt(10.0), 10, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(3 -1,4 -4)",
                  sqrt(5.0), 5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(1 2,2 1)",
                  sqrt(0.5), 0.5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 -2,2 -3)",
                  2, 4, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 -2,0 -3)",
                  sqrt(5.0), 5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 -2,0.5 -3.5)",
                  sqrt(5.0), 5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(-1 -2,0.5 -3.5)",
                  sqrt(5.0), 5, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 3,2.5 2)",
                  sqrt(2.45), 2.45, strategy);
    tester::apply("box(0 0,1 1)",
                  "segment(0.5 1.5,1.5 -1.5)",
                  0, 0, strategy);

    // test degenerate segment
    tester::apply("box(0 0,2 2)",
                  "segment(4 1,4 1)",
                  2, 4, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_linestring_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "linestring/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<linestring_type, box_type> tester;

    // linestrings that intersect the box
    tester::apply("linestring(-1 0.5,0.5 0.75)",
                  "box(0 0,1 1)",
                  0, 0, strategy);
    tester::apply("linestring(-1 0.5,1.5 0.75)",
                  "box(0 0,1 1)",
                  0, 0, strategy);
    
    // linestring that has closest point on box boundary
    tester::apply("linestring(4 0.5,5 0.75)",
                  "box(0 0,1 1)",
                  3, 9, strategy);

    // linestring that has closest point on box corner
    tester::apply("linestring(4 0,0 4)",
                  "box(0 0,1 1)",
                  sqrt(2.0), 2, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multilinestring_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multilinestring/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<multi_linestring_type, box_type> tester;

    // multilinestring that intersects the box
    tester::apply("multilinestring((-1 0.5,0.5 0.75),(4 0.5,5 0.75))",
                  "box(0 0,1 1)",
                  0, 0, strategy);
    
    // multilinestring that has closest point on box boundary
    tester::apply("multilinestring((4 0.5,5 0.75))",
                  "box(0 0,1 1)",
                  3, 9, strategy);

    // multilinestring that has closest point on box corner
    tester::apply("multilinestring((5 0,0 5),(4 0,0 4))",
                  "box(0 0,1 1)",
                  sqrt(2.0), 2, strategy);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_more_empty_input_linear_areal(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::linestring<Point> line_empty;
    bg::model::polygon<Point> polygon_empty;
    bg::model::multi_linestring<bg::model::linestring<Point> > multiline_empty;
    bg::model::multi_polygon<bg::model::polygon<Point> > multipolygon_empty;
    bg::model::ring<Point> ring_empty;

    bg::model::linestring<Point> line =
        from_wkt<bg::model::linestring<Point> >("linestring(0 0,1 1)");

    bg::model::polygon<Point> polygon =
        from_wkt<bg::model::polygon<Point> >("polygon((0 0,1 0,0 1))");

    bg::model::ring<Point> ring =
        from_wkt<bg::model::ring<Point> >("polygon((0 0,1 0,0 1))");

    // 1st geometry is empty
    test_empty_input(line_empty, polygon, strategy);
    test_empty_input(line_empty, ring, strategy);
    test_empty_input(multiline_empty, polygon, strategy);
    test_empty_input(multiline_empty, ring, strategy);

    // 2nd geometry is empty
    test_empty_input(line, polygon_empty, strategy);
    test_empty_input(line, multipolygon_empty, strategy);
    test_empty_input(line, ring_empty, strategy);

    // both geometries are empty
    test_empty_input(line_empty, polygon_empty, strategy);
    test_empty_input(line_empty, multipolygon_empty, strategy);
    test_empty_input(line_empty, ring_empty, strategy);
    test_empty_input(multiline_empty, polygon_empty, strategy);
    test_empty_input(multiline_empty, multipolygon_empty, strategy);
    test_empty_input(multiline_empty, ring_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}

//===========================================================================

BOOST_AUTO_TEST_CASE( test_all_segment_polygon )
{
    test_distance_segment_polygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_linestring_polygon )
{
    test_distance_linestring_polygon(point_segment_strategy());
    test_distance_linestring_open_polygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multilinestring_polygon )
{
    test_distance_multilinestring_polygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_segment_multipolygon )
{
    test_distance_segment_multipolygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_linestring_multipolygon )
{
    test_distance_linestring_multipolygon(point_segment_strategy());
    test_distance_linestring_open_multipolygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multilinestring_multipolygon )
{
    test_distance_multilinestring_multipolygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_segment_ring )
{
    test_distance_segment_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_linestring_ring )
{
    test_distance_linestring_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multilinestring_ring )
{
    test_distance_multilinestring_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_segment_box )
{
    test_distance_segment_box(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_linestring_box )
{
    test_distance_linestring_box(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multilinestring_box )
{
    test_distance_multilinestring_box(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_empty_input_linear_areal )
{
    test_more_empty_input_linear_areal<point_type>(point_segment_strategy());
}
