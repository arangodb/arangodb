// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_pointlike_areal
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_distance_common.hpp"


typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::multi_point<point_type>            multi_point_type;
typedef bg::model::point<double,3,bg::cs::cartesian>  point_type_3d;
typedef bg::model::multi_point<point_type_3d>         multi_point_type_3d;
typedef bg::model::polygon<point_type, false>         polygon_type;
typedef bg::model::multi_polygon<polygon_type>        multi_polygon_type;
typedef bg::model::ring<point_type, false>            ring_type;
typedef bg::model::box<point_type>                    box_type;
typedef bg::model::box<point_type_3d>                 box_type_3d;

namespace services = bg::strategy::distance::services;
typedef bg::default_distance_result<point_type>::type return_type;

typedef bg::strategy::distance::pythagoras<> point_point_strategy;
typedef bg::strategy::distance::projected_point<> point_segment_strategy;
typedef bg::strategy::distance::pythagoras_point_box<> point_box_strategy;

//===========================================================================

template <typename Strategy>
void test_distance_point_polygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/polygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, polygon_type> tester;

    tester::apply("point(0 -20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);

    tester::apply("point(12 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  2, 4, strategy);

    tester::apply("point(0 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("point(0 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10),\
                   (-5 -5,-5 5,5 5,5 -5,-5 -5))",
                  5, 25, strategy);

    // polygons with single-point rings
    tester::apply("point(0 0)",
                  "polygon((-5 5))",
                  sqrt(50.0), 50, strategy);

    tester::apply("point(0 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10),(-5 5))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, ring_type> tester;

    tester::apply("point(0 -20)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  10, 100, strategy);

    tester::apply("point(12 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  2, 4, strategy);

    tester::apply("point(0 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    // single-point rings
    tester::apply("point(0 0)",
                  "polygon((-10 -10))",
                  sqrt(200.0), 200, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<point_type, multi_polygon_type> tester;

    tester::apply("point(0 -20)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((0 22,-1 30,2 40,0 22)))",
                  10, 100, strategy);

    tester::apply("point(12 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  2, 4, strategy);

    tester::apply("point(0 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);

    tester::apply("point(0 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10),\
                   (-5 -5,-5 5,5 5,5 -5,-5 -5)),\
                   ((100 0,101 0,101 1,100 1,100 0)))",
                  5, 25, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_polygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/polygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<multi_point_type, polygon_type> tester;

    tester::apply("multipoint(0 -20,0 -15)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  5, 25, strategy);

    tester::apply("multipoint(16 0,12 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  2, 4, strategy);

    tester::apply("multipoint(0 0,5 5,4 4)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    tester::apply("multipoint(0 0,2 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10),\
                   (-5 -5,-5 5,5 5,5 -5,-5 -5))",
                  3, 9, strategy);

    tester::apply("multipoint(4 4,11 11)",
                  "polygon((0 0,0 10,10 10,10 0,0 0))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_ring(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/ring distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<multi_point_type, ring_type> tester;

    tester::apply("multipoint(0 -20,0 -15)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  5, 25, strategy);

    tester::apply("multipoint(16 0,12 0)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  2, 4, strategy);

    tester::apply("multipoint(0 0,5 5,4 4)",
                  "polygon((-10 -10,10 -10,10 10,-10 10,-10 -10))",
                  0, 0, strategy);

    // single-point ring
    tester::apply("multipoint(0 0,5 5,4 4)",
                  "polygon((10 10))",
                  sqrt(50.0), 50, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_multipolygon(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "multipoint/multipolygon distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            multi_point_type, multi_polygon_type
        > tester;

    tester::apply("multipoint(0 -20,0 -15)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((0 22,-1 30,2 40,0 22)))",
                  5, 25, strategy);

    tester::apply("multipoint(16 0,12 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  2, 4, strategy);

    tester::apply("multipoint(0 0,4 4,5 5)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10)),\
                   ((20 -1,21 2,30 -10,20 -1)))",
                  0, 0, strategy);

    tester::apply("multipoint(0 0,2 0)",
                  "multipolygon(((-10 -10,10 -10,10 10,-10 10,-10 -10),\
                   (-5 -5,-5 5,5 5,5 -5,-5 -5)),\
                   ((100 0,101 0,101 1,100 1,100 0)))",
                  3, 9, strategy);

    tester::apply("MULTIPOINT(19 19,100 100)",
                  "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),\
                  (4 4,4 6,6 6,6 4,4 4)), ((10 10,10 20,20 20,20 10,10 10),\
                  (14 14,14 16,16 16,16 14,14 14)))",
                  0, 0, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_box_2d(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "2D point/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<box_type, point_type> tester;

    // point inside box
    tester::apply("box(-1 -1,1 1)",
                  "point(0 0)", 0, 0, strategy);

    // points on box corners
    tester::apply("box(-1 -1,1 1)",
                  "point(-1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(-1 1)", 0, 0, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(1 1)", 0, 0, strategy);

    // points on box boundary edges
    tester::apply("box(-1 -1,1 1)",
                  "point(0 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(-1 0)", 0, 0, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(1 0)", 0, 0, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(0 1)", 0, 0, strategy);

    // points outside box
    tester::apply("box(-1 -1,1 1)",
                  "point(0 4)", 3, 9, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(0.5 4)", 3, 9, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(-0.5 5)", 4, 16, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(3 0.25)", 2, 4, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(-3 -0.25)", 2, 4, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(3 5)", sqrt(20.0), 20, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(-5 -4)", 5, 25, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(2 -2)", sqrt(2.0), 2, strategy);
    tester::apply("box(-1 -1,1 1)",
                  "point(-3 4)", sqrt(13.0), 13, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_box_different_point_types(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "2D point/box distance tests with different points"
              << std::endl;
#endif
    typedef point_type double_point;
    typedef box_type double_box;
    typedef bg::model::point<int,2,bg::cs::cartesian> int_point;
    typedef bg::model::box<int_point> int_box;

    test_distance_of_geometries
        <
            int_point, int_box
        >::apply("point(0 0)",
                 "box(1 1, 2 2)",
                 sqrt(2.0), 2, strategy);

    test_distance_of_geometries
        <
            double_point, int_box
        >::apply("point(0.5 0)",
                 "box(1 -1,2 1)",
                 0.5, 0.25, strategy);

    test_distance_of_geometries
        <
            double_point, double_box
        >::apply("point(1.5 0)",
                 "box(1 -1,2 1)",
                 0, 0, strategy);

    test_distance_of_geometries
        <
            double_point, int_box
        >::apply("point(1.5 0)",
                 "box(1 -1,2 1)",
                 0, 0, strategy);

    test_distance_of_geometries
        <
            int_point, double_box
        >::apply("point(1 0)",
                 "box(0.5 -1,1.5 1)",
                 0, 0, strategy);

    test_distance_of_geometries
        <
            int_point, double_box
        >::apply("point(0 0)",
                 "box(0.5, -1,1.5, 1)",
                 0.5, 0.25, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_point_box_3d(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "3D point/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<box_type_3d, point_type_3d> tester;

    // point inside box
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 0 0)", 0, 0, strategy);

    // points on box corners
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 -1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 -1 1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 1 1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 -1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 -1 1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 1 1)", 0, 0, strategy);

    // points on box boundary edges
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 -1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 -1 1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 1 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 1 1)", 0, 0, strategy);

    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 0 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 0 1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 0 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 0 1)", 0, 0, strategy);

    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 -1 0)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 1 0)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 -1 0)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 1 0)", 0, 0, strategy);

    // point on box faces
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 0 -1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 0 1)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 -1 0)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 1 0)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-1 0 0)", 0, 0, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(1 0 0)", 0, 0, strategy);

    // points outside box -- closer to box corners
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-2 -3 -4)", sqrt(14.0), 14, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-2 -3 3)", 3, 9, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-2 5 -2)", sqrt(18.0), 18, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-2 5 3)", sqrt(21.0), 21, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(4 -6 -3)", sqrt(38.0), 38, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(4 -6 4)", sqrt(43.0), 43, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(4 7 -2)", sqrt(46.0), 46, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(4 7 8)", sqrt(94.0), 94, strategy);


    // points closer to box facets
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 0 10)", 9, 81, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 0 -5)", 4, 16, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 7 0)", 6, 36, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 -6 0)", 5, 25, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(4 0 0)", 3, 9, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-3 0 0)", 2, 4, strategy);

    // points closer to box edges
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 -4 -5)", 5, 25, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 -3 6)", sqrt(29.0), 29, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 2 -7)", sqrt(37.0), 37, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(0 8 7)", sqrt(85.0), 85, strategy);

    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-4 0 -4)", sqrt(18.0), 18, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-3 0 5)", sqrt(20.0), 20, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(2 0 -6)", sqrt(26.0), 26, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(8 0 6)", sqrt(74.0), 74, strategy);

    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-5 -5 0)", sqrt(32.0), 32, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(-4 6 0)", sqrt(34.0), 34, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(3 -7 0)", sqrt(40.0), 40, strategy);
    tester::apply("box(-1 -1 -1,1 1 1)",
                  "point(9 7 0)", 10, 100, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_box_2d(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "2D multipoint/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries<box_type, multi_point_type> tester;

    // at least one point inside the box
    tester::apply("box(0 0,10 10)",
                  "multipoint(0 0,-1 -1,20 20)",
                  0, 0, strategy);

    tester::apply("box(0 0,10 10)",
                  "multipoint(1 1,-1 -1,20 20)",
                  0, 0, strategy);

    tester::apply("box(0 0,10 10)",
                  "multipoint(1 1,2 2,3 3)",
                  0, 0, strategy);

    // all points outside the box
    tester::apply("box(0 0,10 10)",
                  "multipoint(-1 -1,20 20)",
                  sqrt(2.0), 2, strategy);

    tester::apply("box(0 0,10 10)",
                  "multipoint(5 13, 50 50)",
                  3, 9, strategy);
}

//===========================================================================

template <typename Strategy>
void test_distance_multipoint_box_3d(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "3D multipoint/box distance tests" << std::endl;
#endif
    typedef test_distance_of_geometries
        <
            box_type_3d, multi_point_type_3d
        > tester;

    // at least one point inside the box
    tester::apply("box(0 0 0,10 10 10)",
                  "multipoint(0 0 0,-1 -1 -1,20 20 20)",
                  0, 0, strategy);

    tester::apply("box(0 0 0,10 10 10)",
                  "multipoint(1 1 1,-1 -1 -1,20 20 20)",
                  0, 0, strategy);

    tester::apply("box(0 0 0,10 10 10)",
                  "multipoint(1 1 1,2 2 2,3 3 3)",
                  0, 0, strategy);

    // all points outside the box
    tester::apply("box(0 0 0,10 10 10)",
                  "multipoint(-1 -1 -1,20 20 20)",
                  sqrt(3.0), 3, strategy);

    tester::apply("box(0 0 0,10 10 10)",
                  "multipoint(5 5 13,50 50 50)",
                  3, 9, strategy);
}

//===========================================================================

template <typename Point, typename Strategy>
void test_more_empty_input_pointlike_areal(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "testing on empty inputs... " << std::flush;
#endif
    bg::model::multi_point<Point> multipoint_empty;
    bg::model::polygon<Point> polygon_empty;
    bg::model::multi_polygon<bg::model::polygon<Point> > multipolygon_empty;
    bg::model::ring<Point> ring_empty;

    Point point = from_wkt<Point>("point(0 0)");
    bg::model::polygon<Point> polygon =
        from_wkt<bg::model::polygon<Point> >("polygon((0 0,1 0,0 1))");
    bg::model::ring<Point> ring =
        from_wkt<bg::model::ring<Point> >("polygon((0 0,1 0,0 1))");

    // 1st geometry is empty
    test_empty_input(multipoint_empty, polygon, strategy);
    test_empty_input(multipoint_empty, ring, strategy);

    // 2nd geometry is empty
    test_empty_input(point, polygon_empty, strategy);
    test_empty_input(point, multipolygon_empty, strategy);
    test_empty_input(point, ring_empty, strategy);

    // both geometries are empty
    test_empty_input(multipoint_empty, polygon_empty, strategy);
    test_empty_input(multipoint_empty, multipolygon_empty, strategy);
    test_empty_input(multipoint_empty, ring_empty, strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "done!" << std::endl;
#endif
}

//===========================================================================

BOOST_AUTO_TEST_CASE( test_all_point_polygon )
{
    test_distance_point_polygon(point_point_strategy()); // back-compatibility
    test_distance_point_polygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_point_multipolygon )
{
    test_distance_point_multipolygon(point_point_strategy()); // back-compatibility
    test_distance_point_multipolygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_point_ring )
{
    test_distance_point_ring(point_point_strategy()); // back-compatibility
    test_distance_point_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_polygon )
{
    test_distance_multipoint_polygon(point_point_strategy()); // back-compatibility
    test_distance_multipoint_polygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_multipolygon )
{
    test_distance_multipoint_multipolygon(point_point_strategy()); // back-compatibility
    test_distance_multipoint_multipolygon(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_ring )
{
    test_distance_multipoint_ring(point_segment_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_point_box_2d )
{
    point_box_strategy pb_strategy;

    test_distance_point_box_2d(pb_strategy);
    test_distance_point_box_different_point_types(pb_strategy);
}

BOOST_AUTO_TEST_CASE( test_all_point_box_3d )
{
    test_distance_point_box_3d(point_box_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_box_2d )
{
    test_distance_multipoint_box_2d(point_box_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_multipoint_box_3d )
{
    test_distance_multipoint_box_3d(point_box_strategy());
}

BOOST_AUTO_TEST_CASE( test_all_empty_input_pointlike_areal )
{
    test_more_empty_input_pointlike_areal
        <
            point_type
        >(point_segment_strategy());
}
