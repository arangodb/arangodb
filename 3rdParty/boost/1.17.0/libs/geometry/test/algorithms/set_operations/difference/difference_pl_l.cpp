// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_difference_pointlike_linear
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


BOOST_AUTO_TEST_CASE( test_difference_point_segment )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << "size of std::size_t: " << sizeof(std::size_t) << std::endl;
    std::cout << "size of range_iterator<multipoint>: "
              << sizeof(boost::range_iterator<multi_point_type>::type)
              << std::endl;
    std::cout << "size of range_iterator<multilinestring>: "
              << sizeof(boost::range_iterator<multi_linestring_type>::type)
              << std::endl;
    std::cout << "size of point_iterator<multipoint>: "
              << sizeof(bg::point_iterator<multi_point_type>) << std::endl;
    std::cout << "size of point_iterator<linestring>: "
              << sizeof(bg::point_iterator<linestring_type>) << std::endl;
    std::cout << "size of point_iterator<multilinestring>: "
              << sizeof(bg::point_iterator<multi_linestring_type>) << std::endl;
    std::cout << "size of segment_iterator<linestring>: "
              << sizeof(bg::segment_iterator<linestring_type>) << std::endl;
    std::cout << "size of segment_iterator<multilinestring>: "
              << sizeof(bg::segment_iterator<multi_linestring_type>) << std::endl;

    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / SEGMENT DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef segment_type S;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, S, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("psdf01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("psdf02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("psdf03",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<S>("SEGMENT(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("psdf04",
         from_wkt<P>("POINT(3 3)"),
         from_wkt<S>("SEGMENT(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT(3 3)")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_point_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / LINESTRING DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef linestring_type L;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, L, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("pldf01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("pldf02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pldf03",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<L>("LINESTRING(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pldf04",
         from_wkt<P>("POINT(3 3)"),
         from_wkt<L>("LINESTRING(0 0,2 2)"),
         from_wkt<MP>("MULTIPOINT(3 3)")
         );

    // linestrings with more than two points
    tester::apply
        ("pldf05",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pldf06",
         from_wkt<P>("POINT(2 2)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pldf07",
         from_wkt<P>("POINT(10 10)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT(10 10)")
         );

    tester::apply
        ("pldf08",
         from_wkt<P>("POINT(0 1)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT(0 1)")
         );

    tester::apply
        ("pldf09",
         from_wkt<P>("POINT(4 4)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pldf10",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,4 4)"),
         from_wkt<MP>("MULTIPOINT()")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_point_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** POINT / MULTILINESTRING DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef point_type P;
    typedef multi_linestring_type ML;
    typedef multi_point_type MP;

    typedef test_set_op_of_pointlike_geometries
        <
            P, ML, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("pmldf01",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2))"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("pmldf02",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf03",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 2))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf04",
         from_wkt<P>("POINT(3 3)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 2))"),
         from_wkt<MP>("MULTIPOINT(3 3)")
         );

    // linestrings with more than two points
    tester::apply
        ("pmldf05",
         from_wkt<P>("POINT(1 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,2 2))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf06",
         from_wkt<P>("POINT(2 2)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf07",
         from_wkt<P>("POINT(10 10)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(10 10)")
         );

    tester::apply
        ("pmldf08",
         from_wkt<P>("POINT(0 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 1)")
         );

    tester::apply
        ("pmldf09",
         from_wkt<P>("POINT(4 4)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf10",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    // multilinestrings with more than one linestring
    tester::apply
        ("pmldf11",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((-10,-10),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf12",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,0 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf13",
         from_wkt<P>("POINT(0 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("pmldf14",
         from_wkt<P>("POINT(-20 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(-20 0)")
         );

    tester::apply
        ("pmldf15",
         from_wkt<P>("POINT(0 1)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,10 0),(0 0,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 1)")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_multipoint_segment )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / SEGMENT DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;
    typedef segment_type S;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, S, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("mpsdf01",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("mpsdf02",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsdf03",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsdf04",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsdf05",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<S>("SEGMENT(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)")
         );

    tester::apply
        ("mpsf06",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<S>("SEGMENT(1 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsdf07",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(0 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpsdf08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<S>("SEGMENT(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsdf09",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(-1 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpsdf10",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsdf11",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(-1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpsdf12",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)")
         );

    tester::apply
        ("mpsdf12a",
         from_wkt<MP>("MULTIPOINT(0 0,2 0,0 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)")
         );

    tester::apply
        ("mpsdf12b",
         from_wkt<MP>("MULTIPOINT(0 0,2 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0,2 0)")
         );

    tester::apply
        ("mpsdf12c",
         from_wkt<MP>("MULTIPOINT(0 0,2 0,0 0,2 0,0 0)"),
         from_wkt<S>("SEGMENT(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,0 0,2 0,2 0)")
         );

    tester::apply
        ("mpsdf13",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(2 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpsdf14",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<S>("SEGMENT(0 0,0 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpsdf15",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<S>("SEGMENT(0 0,1 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_multipoint_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / LINESTRING DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;
    typedef linestring_type L;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, L, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("mpldf01",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2,3 3,4 4,5 5)"),
         from_wkt<MP>("MULTIPOINT(0 0)")
         );

    tester::apply
        ("mpldf02",
         from_wkt<MP>("MULTIPOINT(0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpldf03",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpldf04",
         from_wkt<MP>("MULTIPOINT(0 0,0 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpldf05",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<L>("LINESTRING(1 1,2 2)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)")
         );

    tester::apply
        ("mplf06",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,1 0)"),
         from_wkt<L>("LINESTRING(1 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpldf07",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(0 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpldf08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<L>("LINESTRING(0 0,1 1)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpldf09",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(-1 0,1 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpldf10",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpldf11",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(-1 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpldf12",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(3 0,3 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)")
         );

    tester::apply
        ("mpldf13",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(2 0,2 0)"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpldf14",
         from_wkt<MP>("MULTIPOINT(0 0,0 0,2 0)"),
         from_wkt<L>("LINESTRING(0 0,0 0)"),
         from_wkt<MP>("MULTIPOINT(2 0)")
         );

    tester::apply
        ("mpldf15",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<L>("LINESTRING(0 0,1 0,2 0,3 0)"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpldf16",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<L>("LINESTRING()"),
         from_wkt<MP>("MULTIPOINT()")
         );
}


BOOST_AUTO_TEST_CASE( test_difference_multipoint_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTIPOINT / MULTILINESTRING DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_point_type MP;
    typedef multi_linestring_type ML;

    typedef test_set_op_of_pointlike_geometries
        <
            MP, ML, MP, bg::overlay_difference
        > tester;

    tester::apply
        ("mpmldf01",
         from_wkt<MP>("MULTIPOINT(0 0,1 0,2 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 0,2 0)")
         );

    tester::apply
        ("mpmldf02",
         from_wkt<MP>("MULTIPOINT(2 2,3 3,0 0,0 0,2 2,1 1,1 1,1 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(0 0,0 0)")
         );

    tester::apply
        ("mpmldf03",
         from_wkt<MP>("MULTIPOINT(5 5,3 3,0 0,0 0,5 5,1 1,1 1,1 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,1 1,4 4))"),
         from_wkt<MP>("MULTIPOINT(5 5,5 5,0 0,0 0)")
         );

    tester::apply
        ("mpmldf04",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<ML>("MULTILINESTRING((1 0,0 0,1 1,0 0))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmldf05",
         from_wkt<MP>("MULTIPOINT(0 0,1 0,2 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,0 0,1 2,1 0),\
                      (0 1,2 0,0 10,2 0,2 10,5 10,5 0,5 10))"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmldf05a",
         from_wkt<MP>("MULTIPOINT(0 0,1 0,6 0,7 0,2 0,5 0,7 0,8 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,0 0,1 2,1 0),\
                      (0 1,2 0,0 10,2 0,2 10,5 10,5 0,5 10))"),
         from_wkt<MP>("MULTIPOINT(7 0,7 0,8 0,6 0)")
         );

    tester::apply
        ("mpmldf06",
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)"),
         from_wkt<ML>("MULTILINESTRING(())"),
         from_wkt<MP>("MULTIPOINT(0 0,1 1,1 0,1 1)")
         );

    tester::apply
        ("mpmldf07",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<ML>("MULTILINESTRING(())"),
         from_wkt<MP>("MULTIPOINT()")
         );

    tester::apply
        ("mpmldf08",
         from_wkt<MP>("MULTIPOINT()"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(9 0,10 0))"),
         from_wkt<MP>("MULTIPOINT()")
         );
}
