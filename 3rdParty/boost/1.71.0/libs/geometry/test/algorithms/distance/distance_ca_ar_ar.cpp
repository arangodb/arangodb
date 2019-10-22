// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_cartesian_areal_areal
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_common.hpp"


typedef bg::model::point<int,2,bg::cs::cartesian>     int_point_type;
typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::polygon<point_type, false>         polygon_type;
typedef bg::model::multi_polygon<polygon_type>        multi_polygon_type;
typedef bg::model::ring<point_type, false>            ring_type;
typedef bg::model::box<int_point_type>                int_box_type;
typedef bg::model::box<point_type>                    box_type;

namespace services = bg::strategy::distance::services;
typedef bg::default_distance_result<point_type>::type return_type;

typedef bg::strategy::distance::projected_point<> point_segment_strategy;
typedef bg::strategy::distance::pythagoras_box_box<> box_box_strategy;

//===========================================================================

template <typename Strategy>
void test_distance_polygon_polygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "polygon/polygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<polygon_type, polygon_type> tester;

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,5 20,5 25,-5 25,-5 20))",
                  10, 100, strategy);

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,-5 5,5 5,5 20,-5 20))",
                  0, 0, strategy);

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,-5 -20,5 -20,5 20,-5 20))",
                  0, 0, strategy);

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10),\
                   (-5 -5,-5 5,5 5,5 -5,-5 -5))",
                  "polygon((-1 -1,0 0,-1 0,-1 -1))",
                  4, 16, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_polygon_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "polygon/multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            polygon_type, multi_polygon_type
        > tester;

    tester::apply("polygon((12 0,14 0,19 0,19.9 -1,12 0))",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0.1, 0.01, strategy);

    tester::apply("polygon((19 0,19.9 -1,12 0,20.5 0.5,19 0))",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_polygon_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "polygon/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<polygon_type, ring_type> tester;

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,5 20,5 25,-5 25,-5 20))",
                  10, 100, strategy);

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,-5 5,5 5,5 20,-5 20))",
                  0, 0, strategy);

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,-5 -20,5 -20,5 20,-5 20))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipolygon_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipolygon/multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_polygon_type, multi_polygon_type
        > tester;

    tester::apply("multipolygon(((12 0,14 0,14 1,12 0)),\
                   ((18 0,19 0,19.9 -1,18 0)))",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0.1, 0.01, strategy);

    tester::apply("multipolygon(((18 0,19 0,19.9 -1,18 0)),\
                   ((12 0,14 0,20.5 0.5,12 0)))",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipolygon_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipolygon/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_polygon_type, ring_type
        > tester;

    tester::apply("multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  "polygon((12 0,14 0,19 0,19.9 -1,12 0))",
                  0.1, 0.01, strategy);

    tester::apply("multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  "polygon((19 0,19.9 -1,12 0,20.5 0.5,19 0))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_ring_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "ring/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<ring_type, ring_type> tester;

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,5 20,5 25,-5 25,-5 20))",
                  10, 100, strategy);

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,-5 5,5 5,5 20,-5 20))",
                  0, 0, strategy);

    tester::apply("polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  "polygon((-5 20,-5 -20,5 -20,5 20,-5 20))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_box_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "box/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<int_box_type, int_box_type> int_tester;
    typedef test_distance_of_geometries<box_type, box_type> tester;

    int_tester::apply("box(5 5,10 10)",
                      "box(0 0,1 1)",
                      sqrt(32.0), 32, strategy);

    tester::apply("box(5 5,10 10)",
                  "box(0 0,1 1)",
                  sqrt(32.0), 32, strategy);

    tester::apply("box(3 8,13 18)",
                  "box(0 0,5 5)",
                  3, 9, strategy);

    tester::apply("box(5 5,10 10)",
                  "box(0 0,5 5)",
                  0, 0, strategy);

    tester::apply("box(5 5,10 10)",
                  "box(0 0,6 6)",
                  0, 0, strategy);

    tester::apply("box(3 5,13 15)",
                  "box(0 0,5 5)",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_polygon_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "polygon/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<polygon_type, box_type> tester;

    tester::apply("polygon((10 10,10 5,5 5,5 10,10 10))",
                  "box(0 0,1 1)",
                  sqrt(32.0), 32, strategy);

    tester::apply("polygon((10 10,10 5,5 5,5 10,10 10))",
                  "box(0 0,5 5)",
                  0, 0, strategy);

    tester::apply("polygon((10 10,10 5,5 5,5 10,10 10))",
                  "box(0 0,6 6)",
                  0, 0, strategy);

    tester::apply("polygon((10 10,15 5,10 0,5 5,10 10))",
                  "box(5 0,7.5 2.5)",
                  0, 0, strategy);

    tester::apply("polygon((10 10,15 5,10 0,5 5,10 10))",
                  "box(5 0,6 1)",
                  sqrt(4.5), 4.5, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipolygon_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipolygon/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<multi_polygon_type, box_type> tester;

    tester::apply("multipolygon(((-10 -10,-10 -9,-9 -9,-9 -10,-10 -10)),\
                   ((2 2,2 3,3 3,3 2,2 2)))",
                  "box(0 0,1 1)",
                  sqrt(2.0), 2, strategy);

    tester::apply("multipolygon(((-10 -10,-10 -9,-9 -9,-9 -10,-10 -10)),\
                   ((2 2,2 3,3 3,3 2,2 2)))",
                  "box(0 0,2 2)",
                  0, 0, strategy);

    tester::apply("multipolygon(((-10 -10,-10 -9,-9 -9,-9 -10,-10 -10)),\
                   ((2 2,2 3,3 3,3 2,2 2)))",
                  "box(0 0,2.5 2)",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_ring_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "ring/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<ring_type, box_type> tester;

    tester::apply("polygon((10 10,10 5,5 5,5 10,10 10))",
                  "box(0 0,1 1)",
                  sqrt(32.0), 32, strategy);

    tester::apply("polygon((10 10,10 5,5 5,5 10,10 10))",
                  "box(0 0,5 5)",
                  0, 0, strategy);

    tester::apply("polygon((10 10,10 5,5 5,5 10,10 10))",
                  "box(0 0,6 6)",
                  0, 0, strategy);

    tester::apply("polygon((10 10,15 5,10 0,5 5,10 10))",
                  "box(5 0,7.5 2.5)",
                  0, 0, strategy);

    tester::apply("polygon((10 10,15 5,10 0,5 5,10 10))",
                  "box(5 0,6 1)",
                  sqrt(4.5), 4.5, strategy);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_more_empty_input_areal_areal(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::polygon<Point> polygon_empty;
    bg::model::multi_polygon<bg::model::polygon<Point> > multipolygon_empty;
    bg::model::ring<Point> ring_empty;

    bg::model::polygon<Point> polygon =
        from_wkt<bg::model::polygon<Point> >("polygon((0 0,1 0,0 1))");

    bg::model::multi_polygon<bg::model::polygon<Point> > multipolygon =
        from_wkt
            <
                bg::model::multi_polygon<bg::model::polygon<Point> >
            >("multipolygon(((0 0,1 0,0 1)))");

    bg::model::ring<Point> ring =
        from_wkt<bg::model::ring<Point> >("polygon((0 0,1 0,0 1))");

    // 1st geometry is empty
    test_empty_input(polygon_empty, polygon, strategy);
    test_empty_input(polygon_empty, multipolygon, strategy);
    test_empty_input(polygon_empty, ring, strategy);
    test_empty_input(multipolygon_empty, polygon, strategy);
    test_empty_input(multipolygon_empty, multipolygon, strategy);
    test_empty_input(multipolygon_empty, ring, strategy);
    test_empty_input(ring_empty, polygon, strategy);
    test_empty_input(ring_empty, multipolygon, strategy);
    test_empty_input(ring_empty, ring, strategy);

    // 2nd geometry is empty
    test_empty_input(polygon, polygon_empty, strategy);
    test_empty_input(polygon, multipolygon_empty, strategy);
    test_empty_input(polygon, ring_empty, strategy);
    test_empty_input(multipolygon, polygon_empty, strategy);
    test_empty_input(multipolygon, multipolygon_empty, strategy);
    test_empty_input(multipolygon, ring_empty, strategy);
    test_empty_input(ring, polygon_empty, strategy);
    test_empty_input(ring, multipolygon_empty, strategy);
    test_empty_input(ring, ring_empty, strategy);

    // both geometries are empty
    test_empty_input(polygon_empty, polygon_empty, strategy);
    test_empty_input(polygon_empty, multipolygon_empty, strategy);
    test_empty_input(polygon_empty, ring_empty, strategy);
    test_empty_input(multipolygon_empty, polygon_empty, strategy);
    test_empty_input(multipolygon_empty, multipolygon_empty, strategy);
    test_empty_input(multipolygon_empty, ring_empty, strategy);
    test_empty_input(ring_empty, polygon_empty, strategy);
    test_empty_input(ring_empty, multipolygon_empty, strategy);
    test_empty_input(ring_empty, ring_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}

//===========================================================================

BOOST_AUTO_TEST_CASE( test_all_polygon_polygon )
{
    test_distance_polygon_polygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_polygon_multipolygon )
{
    test_distance_polygon_multipolygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_polygon_ring )
{
    test_distance_polygon_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipolygon_multipolygon )
{
    test_distance_multipolygon_multipolygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipolygon_ring )
{
    test_distance_multipolygon_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_ring_ring )
{
    test_distance_ring_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_box_box )
{
    test_distance_box_box(box_box_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_polygon_box )
{
    test_distance_polygon_box(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipolygon_box )
{
    test_distance_multipolygon_box(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_ring_box )
{
    test_distance_ring_box(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_empty_input_areal_areal )
{
    test_more_empty_input_areal_areal<point_type>(point_segment_strategy());
}
