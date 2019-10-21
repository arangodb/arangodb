// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_intersection_pointlike_linear
#endif

#include <iostream>
#include <algorithm>

#include <boost/test/included/unit_test.hpp>

#include "../test_set_ops_pointlike.hpp"

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>

typedef bg::model::point<double, 2, bg::cs::cartesian>  point_type;
typedef bg::model::segment<point_type>  segment_type;
typedef bg::model::linestring<point_type>  linestring_type;
typedef bg::model::multi_point<point_type>  multi_point_type;
typedef bg::model::multi_linestring<linestring_type>  multi_linestring_type;


//===========================================================================
//===========================================================================
//===========================================================================


BOOST_AUTO_TEST_CASE( test_intersection_point_segment )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / SEGMENT INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef segment_type S;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, S, MP, bg::overlay_intersection
        > tester;

    tester::apply
        ("psi01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("psi02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("psi03",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<S>("SEGMENT(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("psi04",
         from_wkt<P>("POINT(3 3)"),
         from_wkt<S>("SEGMENT(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_point_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / LINESTRING INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef linestring_type L;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, L, MP, bg::overlay_intersection
        > tester;

    tester::apply
        ("pli01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pli02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("pli03",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<L>("LINESTRING(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("pli04",
         from_wkt<P>("POINT(3 3)"),
         from_wkt<L>("LINESTRING(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    // linestrings with more than two points
    tester::apply
        ("pli05",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("pli06",
         from_wkt<P>("POINT(2 2)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT(2 2)")
         );

    tester::apply
        ("pli07",
         from_wkt<P>("POINT(10 10)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pli08",
         from_wkt<P>("POINT(0 1)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pli09",
         from_wkt<P>("POINT(4 4)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT(4 4)")
         );

    tester::apply
        ("pli10",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_point_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / MULTILINESTRING INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef multi_linestring_type ML;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, ML, MP, bg::overlay_intersection
        > tester;

    tester::apply
        ("pmli01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmli02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1))"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("pmli03",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 2))"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("pmli04",
         from_wkt<P>("POINT(3 3)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 2))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    // linestrings with more than two points
    tester::apply
        ("pmli05",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,2 2))"),
         from_wkt<MP>("MULTIPOINT(1 1)")
         );

    tester::apply
        ("pmli06",
         from_wkt<P>("POINT(2 2)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(2 2)")
         );

    tester::apply
        ("pmli07",
         from_wkt<P>("POINT(10 10)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmli08",
         from_wkt<P>("POINT(0 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmli09",
         from_wkt<P>("POINT(4 4)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(4 4)")
         );

    tester::apply
        ("pmli10",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    // multilinestrings with more than one linestring
    tester::apply
        ("pmli11",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((-10,-10),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("pmli12",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,0 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("pmli13",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("pmli14",
         from_wkt<P>("POINT(-20 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmli15",
         from_wkt<P>("POINT(0 1)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmli16",
         from_wkt<P>("POINT(1 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(-1 0,2 0,20 0))"),
         from_wkt<MP>("MULTIPOINT(1 0)")
         );

    tester::apply
        ("pmli17",
         from_wkt<P>("POINT(1 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(0 -1,0 2,0 4))"),
         from_wkt<MP>("MULTIPOINT(1 0)")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_multipoint_segment )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / SEGMENT INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;
    typedef segment_type S;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, S, MP, bg::overlay_intersection
        > tester;

    tester::apply
        ("mpsi01",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi02",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("mpsi03",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi04",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsi05",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsf06",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<S>("SEGMENT(1 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(1 0)")
         );

    tester::apply
        ("mpsi07",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(0 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsi07a",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(0 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)")
         );

    tester::apply
        ("mpsi08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi09",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(-1 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsi10",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpsi11",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(-1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)")
         );

    tester::apply
        ("mpsi12",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi12a",
         from_wkt<MP>("MULTIPOINT(0 0,2 0,0 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi12b",
         from_wkt<MP>("MULTIPOINT(0 0,2 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi12c",
         from_wkt<MP>("MULTIPOINT(0 0,2 0,0 0,2 0,0 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi13",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(2 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpsi14",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(0 0,0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsi15",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<S>("SEGMENT(0 0,1 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsi16",
         from_wkt<MP>("MULTIPOINT(0 0,2 0,0 0,2 0,0 0)"),
         from_wkt<S>("SEGMENT(-1 0,10 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,0 0,2 0,2 0)")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_multipoint_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / LINESTRING INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;
    typedef linestring_type L;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, L, MP, bg::overlay_intersection
        > tester;

    tester::apply
        ("mpli01",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2,3 3,4 4,5 5)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpli02",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("mpli03",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpli04",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpli05",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpli06",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<L>("LINESTRING(1 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(1 0)")
         );

    tester::apply
        ("mpli07",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(0 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpli08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpli09",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(-1 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpli10",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpli10a",
         from_wkt<MP>("MULTIPOINT(2 0,0 0,2 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(2 0,2 0,2 0)")
         );

    tester::apply
        ("mpli11",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(-1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(2 0,0 0,0 0)")
         );

    tester::apply
        ("mpli12",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpli13",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(2 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpli14",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(0 0,0 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpli15",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<L>("LINESTRING(0 0,1 0,2 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpli16",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<L>("LINESTRING()"),
         from_wkt<MP>("MULTIPOINT()")
         );
}


BOOST_AUTO_TEST_CASE( test_intersection_multipoint_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / MULTILINESTRING INTERSECTION ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;
    typedef multi_linestring_type ML;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, ML, MP, bg::overlay_intersection
        > tester;

    tester::apply
        ("mpmli01",
         from_wkt<MP>("MULTIPOINT(0 0,1 0,2 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(1 0)")
         );

    tester::apply
        ("mpmli02",
         from_wkt<MP>("MULTIPOINT(2 2,3 3,0 0,0 0,2 2,1 1,1 1,1 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(2 2,2 2,3 3,1 1,1 1,1 0,1 0)")
         );

    tester::apply
        ("mpmli03",
         from_wkt<MP>("MULTIPOINT(5 5,3 3,0 0,0 0,5 5,1 1,1 1,1 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(1 0,1 0,3 3,1 1,1 1)")
         );

    tester::apply
        ("mpmli04",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<ML>("MULTILINESTRING((1 0,0 0,1 1,0 0))"),
         from_wkt<MP>("MULTIPOINT(1 0,0 0,1 1,1 1)")
         );

    tester::apply
        ("mpmli05",
         from_wkt<MP>("MULTIPOINT(0 0,1 0,2 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,0 0,1 2,1 0),\
                      (0 1,2 0,0 10,2 0,2 10,5 10,5 0,5 10))"),
         from_wkt<MP>("MULTIPOINT(0 0,1 0,2 0,5 0)")
         );

    tester::apply
        ("mpmli06",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<ML>("MULTILINESTRING(())"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmli07",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<ML>("MULTILINESTRING(())"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmli08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(9 0,10 0))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmli09",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,0 0,0 0,0 0,0 0,0 0,0 0,\
                      0 0,0 0,0 0,0 0,0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,0 1),(0 0,2 0),(0 0,0 3),\
                      (0 0,4 0),(0 0,0 5),(0 0,6 0),(0 0,7 0),(0 0,8 0))"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,0 0,0 0,0 0,0 0,0 0,\
                      0 0,0 0,0 0,0 0,0 0)")
         );

    tester::apply
        ("mpmli09a",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,0 1),(0 0,2 0),(0 0,0 3),\
                      (0 0,4 0),(0 0,0 5),(0 0,6 0),(0 0,7 0),(0 0,8 0))"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );
}
