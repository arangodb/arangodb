// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_intersection_linear_linear
#endif

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#define BOOST_GEOMETRY_DEBUG_TURNS
#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_intersection_linear_linear.hpp"

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/algorithms/intersection.hpp>

typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::segment<point_type>                segment_type;
typedef bg::model::linestring<point_type>             linestring_type;
typedef bg::model::multi_linestring<linestring_type>  multi_linestring_type;



//===========================================================================
//===========================================================================
//===========================================================================


BOOST_AUTO_TEST_CASE( test_intersection_linestring_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** LINESTRING / LINESTRING INTERSECTION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_intersection_of_geometries<L, L, ML> tester;

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 1,2 1,3 2)"),
         from_wkt<L>("LINESTRING(0 2,1 1,2 1,3 0)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 1))"),
         "lli00");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,5 0)"),
         from_wkt<L>("LINESTRING(3 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((3 0,4 0))"),
         "lli01");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,6 0)"),
         from_wkt<ML>("MULTILINESTRING((3 0,4 0))"),
         "lli01-2");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,6 0)"),
         from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,4 0))"),
         "lli01-4");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((4 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(2 0),(4 0,5 0),(6.5 0),(8 0))"),
#endif
         "lli01-6");

    tester::apply
        (from_wkt<L>("LINESTRING(-20 0,20 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((4 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(2 0),(4 0,5 0),(6.5 0),(8 0))"),
#endif
         "lli01-7");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<L>("LINESTRING(2 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0))"),
         "lli01-8");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0)"),
         from_wkt<L>("LINESTRING(4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING()"),
         "lli01-10");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0)"),
         from_wkt<L>("LINESTRING(2 0,5 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((2 0))"),
#endif
         "lli01-11");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((3 0,4 0))"),
         "lli01-11a");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((3 0,4 0))"),
         "lli01-11b");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,5 0,10 0)"),
         from_wkt<L>("LINESTRING(2 0,6 0,8 0)"),
         from_wkt<ML>("MULTILINESTRING((2 0,5 0,8 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,6 0,8 0))"),
         "lli01-11c");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,6 0)"),
         from_wkt<L>("LINESTRING(2 0,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((2 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0,5 0))"),
         "lli01-12");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,5 5,10 5,15 0)"),
         from_wkt<L>("LINESTRING(-1 6,0 5,15 5)"),
         from_wkt<ML>("MULTILINESTRING((5 5,10 5))"),
         "lli02");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0))"),
         "lli03");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,5 5,10 5,15 0,20 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0))"),
         "lli04");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,25 1)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0)(15 0,20 0))"),
         "lli05");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,30 0))"),
         "lli05-1");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,30 0))"),
         "lli06");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,25 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,25 0,30 0))"),
         "lli07");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,19 0,30 0))"),
         "lli08");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,30 0))"),
         "lli09");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(15 0,30 0))"),
         "lli10");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,20 0,\
                                 30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0),(3 0,4 0),\
                      (15 0,20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0),(3 0,4 0),\
                      (15 0,30 0))"),
         "lli11");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0),\
                      (3 0,4 0),(15 0,30 0))"),
         "lli11-1");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0,3 1))"),
         "lli12");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<L>("LINESTRING(3 1,2 0,0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((3 1,2 0,0 0))"),
         "lli12-1");

   tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 5,4 0)"),
         from_wkt<L>("LINESTRING(1 0,2 1,3 5,4 0,5 10)"),
         from_wkt<ML>("MULTILINESTRING((1 0,2 1,3 5,4 0))"),
         "lli13");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 0,2.5 0,3 1)"),
         from_wkt<L>("LINESTRING(0 0,2 0,2.5 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0,2.5 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0,2.5 0,3 1))"),
         "lli14");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 5,4 0)"),
         from_wkt<L>("LINESTRING(1 0,2 1,3 5)"),
         from_wkt<ML>("MULTILINESTRING((1 0,2 1,3 5))"),
         "lli15");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 2)"),
         from_wkt<L>("LINESTRING(0.5 0,1 0,3 2,4 5)"),
         from_wkt<ML>("MULTILINESTRING((0.5 0,1 0,2 1,3 2))"),
         from_wkt<ML>("MULTILINESTRING((0.5 0,1 0,3 2))"),
         "lli16");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 2)"),
         from_wkt<L>("LINESTRING(4 5,3 2,1 0,0.5 0)"),
         from_wkt<ML>("MULTILINESTRING((0.5 0,1 0,2 1,3 2))"),
         from_wkt<ML>("MULTILINESTRING((0.5 0,1 0,3 2))"),
         "lli16-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1,30 1)"),
         from_wkt<L>("LINESTRING(1 1,2 0,3 1,20 1,25 1)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((20 1,25 1))"),
#else
         from_wkt<ML>("MULTILINESTRING((2 0),(20 1,25 1))"),
#endif
         "lli17");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1,21 0,30 0)"),
         from_wkt<L>("LINESTRING(1 1,2 0,3 1,20 1,25 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((2 0),(20 1),(25 0))"),
#endif
         "lli18");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 1)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0))"),
#endif
         "lli19");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,4 0,4 1,20 1,5 0,1 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0))"),
#endif
         "lli19-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0))"),
#endif
         "lli19a");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(4 0,4 1,20 1,5 0,1 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0))"),
#endif
         "lli19a-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0,5 0))"),
         "lli19b");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0,6 1)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0,5 0))"),
         "lli19c");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,3 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0,3 0))"),
         "lli19d");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,3 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0,3 0))"),
         "lli19e");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0,5 1)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(4 0,5 0))"),
         "lli19f");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,5 0,4 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,4 0),(5 0,1 0))"),
         "lli19f-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,5 0,5 1)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(5 0))"),
#endif
         "lli19g");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,5 0,4 1,20 1,5 0,1 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0),(5 0))"),
#endif
         "lli19g-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0,30 30,10 30,10 -10,15 0,40 0)"),
         from_wkt<L>("LINESTRING(5 5,10 0,10 30,20 0,25 0,25 25,50 0,35 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((20 0,25 0),(10 30,10 0),\
                      (35 0,40 0),(20 0,25 0))"),
         from_wkt<ML>("MULTILINESTRING((20 0,25 0),(10 0,10 30),\
                      (40 0,35 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((10 0),(20 0,25 0),(10 30,10 0),\
                      (30 20),(35 0,40 0),(20 0,25 0))"),
         from_wkt<ML>("MULTILINESTRING((10 0),(20 0,25 0),(10 0,10 30),\
                      (30 20),(40 0,35 0))"),
#endif
         "lli20");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0,30 30,10 30,10 -10,15 0,40 0)"),
         from_wkt<L>("LINESTRING(5 5,10 0,10 30,20 0,25 0,25 25,50 0,15 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((15 0,30 0),(10 30,10 0),\
                      (15 0,40 0))"),
         from_wkt<ML>("MULTILINESTRING((10 0,10 30),(20 0,25 0),(40 0,15 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((10 0),(15 0,30 0),(10 30,10 0),\
                      (30 20),(15 0,40 0))"),
         from_wkt<ML>("MULTILINESTRING((10 0),(10 0,10 30),(20 0,25 0),\
                      (30 20),(40 0,15 0))"),
#endif
         "lli20a");


    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<L>("LINESTRING(2 2,5 -1,15 2,18 0,20 0)"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((18 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((4 0),(8.33333333333333333 0),\
                      (18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((4 0),(8.33333333333333333 0),\
                      (18 0,20 0))"),
#endif
         "lli21"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0)"),
         from_wkt<L>("LINESTRING(1 0,4 0,2 1,5 1,4 0,8 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,4 0),(4 0,8 0))"),
         "lli22"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0)"),
         from_wkt<L>("LINESTRING(4 0,5 0,5 1,1 1,1 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,4 0),(4 0,5 0))"),
         "lli23"
         );

    // the following two tests have been discussed with by Adam
    tester::apply
        (from_wkt<L>("LINESTRING(1 0,1 1,2 1)"),
         from_wkt<L>("LINESTRING(2 1,1 1,1 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1))"),
         "lli24"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(1 0,1 1,2 1)"),
         from_wkt<L>("LINESTRING(1 2,1 1,1 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1))"),
         "lli25"
         );
}




BOOST_AUTO_TEST_CASE( test_intersection_linestring_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** LINESTRING / MULTILINESTRING INTERSECTION ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_intersection_of_geometries<L, ML, ML> tester;

    // the inertsection code automatically reverses the order of the
    // geometries according to the geometry IDs.
    // all calls below are actually reversed, and internally the
    // intersection of the linestring with the multi-linestring is
    // computed.

    // disjoint linestrings
    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 3),(1 1,2 2,5 3))"),
         from_wkt<ML>("MULTILINESTRING()"),
         "lmli01"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0))"),
         "lmli02"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,5 0))"),
         "lmli03"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0))"),
         "lmli04"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,101 0))"),
         "lmli07"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         "lmli07a"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         "lmli07b"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,101 0))"),
         "lmli08"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 0.5,3 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((3 0,101 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0),(2 0.5),(3 0,101 0))"),
#endif
         "lmli09"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,1.5 0,2 0.5,3 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,1 0,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,1.5 0),(3 0,101 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0),(2 0.5),(1 0,1.5 0),(3 0,101 0))"),
#endif
         "lmli10"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (1 1,2 0,18 0,19 1),(2 1,3 0,17 0,18 1),\
                      (3 1,4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli12"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0,20 1),\
                      (2 0,18 0,19 1),(3 0,17 0,18 1),\
                      (4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli13"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli14"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,6 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli15"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli15a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,5 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli16"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,5 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli16a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(25 0))"),
#endif
         "lmli17"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0,26 2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(25 0))"),
#endif
         "lmli17a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         "lmli18"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,18 0,19 0))"),
         "lmli18a"
         );
}






#ifndef BOOST_GEOMETRY_TEST_NO_DEGENERATE
BOOST_AUTO_TEST_CASE( test_intersection_l_ml_degenerate )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** LINESTRING / MULTILINESTRING INTERSECTION"
              << " (DEGENERATE) ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_intersection_of_geometries<L, ML, ML> tester;

    // the following test cases concern linestrings with duplicate
    // points and possibly linestrings with zero length.

    // no unique: (3 0) appears twice
    tester::apply
        (from_wkt<L>("LINESTRING(0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (4 0,4 10,4 10))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(1 0),(2 0),(3 0),(4 0),\
                      (5 0,18 0,19 0,20 0))"),
#endif
         "lmli20a"
         );

    // no unique: (3 0) appears twice
    tester::apply
        (from_wkt<L>("LINESTRING(0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (4 0,4 0,4 10,4 10))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(1 0),(2 0),(3 0),(4 0),\
                      (5 0,18 0,19 0,20 0))"),
#endif
         "lmli20b"
         );

    // no unique: (3 0) appears twice
    tester::apply
        (from_wkt<L>("LINESTRING(0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (30 0,30 0,30 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(1 0),(2 0),(3 0),\
                      (5 0,18 0,19 0,20 0),(30 0))"),
#endif
         "lmli20c"
         );

    // no unique: (3 0) appears twice
    tester::apply
        (from_wkt<L>("LINESTRING(0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (30 0,30 0,31 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(1 0),(2 0),(3 0),\
                      (5 0,18 0,19 0,20 0),(30 0))"),
#endif
         "lmli20d"
         );
}
#endif // BOOST_GEOMETRY_TEST_NO_DEGENERATE




BOOST_AUTO_TEST_CASE( test_intersection_multilinestring_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / LINESTRING INTERSECTION ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_intersection_of_geometries<ML, L, ML> tester;

    // the intersection code automatically reverses the order of the
    // geometries according to the geometry IDs.
    // all calls below are actually reversed, and internally the
    // intersection of the linestring with the multi-linestring is
    // computed.

    // disjoint linestrings
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<L>("LINESTRING(1 1,2 2,4 3)"),
         from_wkt<ML>("MULTILINESTRING()"),
         "mlli01"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<L>("LINESTRING(1 1,2 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0))"),
         "mlli02"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<L>("LINESTRING(-1 -1,1 0,101 0,200 -1)"),
         from_wkt<ML>("MULTILINESTRING((1 0,101 0))"),
         "mlli03"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<L>("LINESTRING(0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(18 0,2 0),\
                       (3 0,17 0),(16 0,4 0))"),
         "mlli04"
         );
}






BOOST_AUTO_TEST_CASE( test_intersection_multilinestring_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / MULTILINESTRING INTERSECTION ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_linestring_type ML;

    typedef test_intersection_of_geometries<ML, ML, ML> tester;

    // disjoint linestrings
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 3),(1 1,2 2,5 3))"),
         from_wkt<ML>("MULTILINESTRING()"),
         "mlmli01"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(3 0,4 0))"),
         "mlmli02"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,5 0),(2 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(3 0,5 0))"),
         "mlmli03"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0))"),
         "mlmli04"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                       (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                       (10 20,15 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(2 0,4 0),(15 10,20 10))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(15 10,20 10))"),
         "mlmli05"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 10),(1 0,7 0),\
                       (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                       (-1 -1,0 0,9 0,11 10,12 10,13 3,14 4,15 5),\
                       (10 20,15 10,25 10,30 15))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((0 0,9 0),(13 3,15 5),\
                      (1 0,7 0),(11 10,12 10),(15 10,20 10))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(0 0,9 0),(13 3,14 4,15 5),\
                      (11 10,12 10),(15 10,20 10))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0,9 0),(13 3,15 5),(20 10),\
                      (1 0,7 0),(11 10,12 10),(15 10,20 10))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(0 0,9 0),(13 3,14 4,15 5),\
                      (11 10,12 10),(15 10,20 10))"),
#endif
         "mlmli06"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,101 0))"),
         "mlmli07"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((-1 1,0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,50 0),(20 0,101 0))"),
         "mlmli07a"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,50 0),(20 0,101 0))"),
         "mlmli07b"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0),\
                       (-1 -1,1 0,101 0,200 -1))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,101 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0),(1 0,101 0))"),
#endif
         "mlmli08"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0.5,3 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((3 0,101 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0),(2 0.5),(3 0,101 0))"),
#endif
         "mlmli09"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 0,1.5 0,2 0.5,3 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,1 0,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,1.5 0),(3 0,101 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,1.5 0),(2 0.5),(3 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0),(1 0,1.5 0),(2 0.5),(3 0,101 0))"),
#endif
         "mlmli10"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 1,100 1,101 0),\
                       (0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1,3 0,4 0,5 1,6 1,\
                       7 0,8 0,9 1,10 1,11 0,12 0,13 1,14 1,15 0),\
                       (-1 -1,1 0,101 0,200 -1))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 1,2 1),(5 1,6 1),(9 1,10 1),\
                       (13 1,14 1),(1 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 1),(5 1,6 1),(9 1,10 1),\
                       (13 1,14 1),(3 0,4 0),(7 0,8 0),(11 0,12 0),\
                       (1 0,101 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 1,2 1),(5 1,6 1),(9 1,10 1),\
                       (13 1,14 1),(101 0),(1 0),(1 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0),(1 1,2 1),(5 1,6 1),(9 1,10 1),\
                       (13 1,14 1),(3 0,4 0),(7 0,8 0),(11 0,12 0),(15 0),\
                       (1 0,101 0))"),
#endif
         "mlmli11"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (1 1,2 0,18 0,19 1),(2 1,3 0,17 0,18 1),\
                      (3 1,4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(2 0,18 0),(3 0,17 0),\
                      (4 0,16 0))"),
         "mlmli12"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0,20 1),\
                      (2 0,18 0,19 1),(3 0,17 0,18 1),\
                      (4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(2 0,18 0),(3 0,17 0),\
                      (4 0,16 0))"),
         "mlmli13"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(18 0,2 0),\
                       (3 0,17 0),(16 0,4 0))"),
         "mlmli14"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,6 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(6 0))"),
#endif
         "mlmli15"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,4 2,2 2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(6 0))"),
#endif
         "mlmli15a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,5 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(5 0,6 0))"),
         "mlmli16"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,5 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(6 0,5 0))"),
         "mlmli16a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(25 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(4 0),(25 0))"),
#endif
         "mlmli17"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0,26 2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(25 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(4 0),(25 0))"),
#endif
         "mlmli17a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(4 0),\
                      (8.3333333333333333333 0),(18 0))"),
#endif
         "mlmli18"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,18 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,18 0,19 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(4 0),\
                      (8.3333333333333333333 0),(18 0))"),
#endif
         "mlmli18a"
         );
}






#ifndef BOOST_GEOMETRY_TEST_NO_DEGENERATE
BOOST_AUTO_TEST_CASE( test_intersection_ml_ml_degenerate )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / MULTILINESTRING INTERSECTION"
              << " (DEGENERATE) ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_linestring_type ML;

    typedef test_intersection_of_geometries<ML, ML, ML> tester;

    // the following test cases concern linestrings with duplicate
    // points and possibly linestrings with zero length.

    // no unique: (3 0) appears twice
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((5 5,5 5),(0 0,18 0,18 0,\
                      19 0,19 0,19 0,30 0),(2 0,2 0),(4 10,4 10))"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 10),\
                      (4 0,4 10),(5 5,5 5))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((5 5),(0 0),(1 0),(2 0),(3 0),\
                      (4 0),(5 0,18 0,19 0,20 0),(2 0),(4 10))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0),(1 0),(2 0),(2 0),(3 0),\
                      (0 0),(4 0),(4 10),(5 5))"),
#endif
         "mlmli20a"
         );

    // no unique: (3 0) appears three times
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((5 5,5 5),(0 0,0 0,18 0,18 0,\
                      19 0,19 0,19 0,30 0,30 0),(2 0,2 0),(4 10,4 10))"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (4 0,4 10,4 10),(5 5,5 5))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((5 5),(0 0),(1 0),(2 0),(3 0),(4 0),\
                      (5 0,18 0,19 0,20 0),(2 0),(4 10))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0),(1 0),(2 0),(2 0),\
                      (3 0),(0 0),(4 0),(4 10),(5 5))"),
#endif
         "mlmli20aa"
         );

    // no unique: (3 0) appears twice
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((5 5,5 5),(0 0,0 0,18 0,18 0,\
                      19 0,19 0,19 0,30 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (4 0,4 0,4 10,4 10),(0 5,15 5))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((5 5),(0 0),(1 0),(2 0),(3 0),(4 0),\
                      (5 0,18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0),(1 0),(2 0),(3 0),\
                      (0 0),(4 0),(5 5))"),
#endif
         "mlmli20b"
         );

    // no unique: (3 0) and (30 0) appear twice
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,0 0,18 0,18 0,\
                      19 0,19 0,19 0,30 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (30 0,30 0,30 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(1 0),(2 0),(3 0),\
                      (5 0,18 0,19 0,20 0),(30 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0),(1 0),(2 0),(3 0),\
                      (0 0),(30 0))"),
#endif
         "mlmli20c"
         );

    // no unique: (3 0) appears twice
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,0 0,18 0,18 0,\
                      19 0,19 0,19 0,30 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((-10 0,-9 0),(0 10,5 0,20 0,20 0,30 10),\
                      (1 1,1 1,2 2,2 2),(1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (30 0,30 0,31 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(1 0),(2 0),(3 0),\
                      (5 0,18 0,19 0,20 0),(30 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0),(1 0),(2 0),(3 0),\
                      (0 0),(30 0))"),
#endif
         "mlmli20d"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,0 0,18 0,18 0,\
                      19 0,19 0,19 0,30 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 10,5 0,20 0,20 0,30 0),\
                      (1 10,1 10,1 0,1 0,1 -10),\
                      (2 0,2 0),(3 0,3 0,3 0),(0 0,0 0,0 10,0 10),\
                      (30 0,30 0,31 0,31 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((5 0,18 0,19 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0,30 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(1 0),(2 0),(3 0),\
                      (5 0,18 0,19 0,30 0),(30 0))"),
         from_wkt<ML>("MULTILINESTRING((5 0,20 0,30 0),(1 0),(2 0),(3 0),\
                      (0 0),(30 0))"),
#endif
         "mlmli20e"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((1 5, -4.3 -.1), (0 6, 8.6 6, 189.7654 5, 1 3, 6 3, 3 5, 6 2.232432, 0 4), (-6 5, 1 2.232432), (3 -1032.34324, 9 0, 189.7654 1, -1.4 3, 3 189.7654, +.3 10.0002, 1 5, 6 3, 5 1, 9 1, 10.0002 -1032.34324, -0.7654 0, 5 3, 3 4), (2.232432 2.232432, 8.6 +.4, 0.0 2.232432, 4 0, -8.8 10.0002), (1 0, 6 6, 7 2, -0 8.4), (-0.7654 3, +.6 8, 4 -1032.34324, 1 6, 0 4), (0 7, 2 1, 8 -7, 7 -.7, -1032.34324 9), (5 0, 10.0002 4, 8 7, 3 3, -8.1 5))"),
         from_wkt<ML>("MULTILINESTRING((5 10.0002, 2 7, -0.7654 0, 5 3), (0 -0.7654, 4 10.0002, 4 +.1, -.8 3, -.1 8, 10.0002 2, +.9 -1032.34324))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((-0.7654 8.88178e-16,-0.7654 0,5 3))"),
#else
         from_wkt<ML>("MULTILINESTRING((-0.756651 3.30964),(1.60494 6),\
                      (2.51371 6),(3.26673 6),(4 6),(8.18862 3.07616),\
                      (4 3.03179),(1.40063 3.00424),(1.39905 3),\
                      (4 3),(5 3),(4 4.33333),(4 4.07748),\
                      (4.41962 2.698),(4 2.82162),(1.59592 3.52985),\
                      (0.729883 3.78498),(-0.532243 2.83823),\
                      (0.235887 2.53454),(7.08745 -329.0674155),\
                      (9.98265 0.00543606),(8.49103 2.89652),\
                      (4.87386 2.93436),(4 2.9435),(1.38821 2.97083)\
                      (0.412281 2.98104),(-0.789427 2.99361),\
                      (0.641699 7.5594),(1.18124 4.9275),\
                      (1.99437 4.60225),(4 3.8),(9.09826 -100.515944),\
                      (5.06428 -559.024344),\
                      (4 3.5),(3.06464 1.99294),(4 1.72377),\
                      (4 1.38014),(2.50083 1.69957),(1.03214 2.01251),\
                      (0.72677 2.07758),(0.10749 2.20953),\
                      (0.0954852 2.17914),(0.92255 1.71755),\
                      (1.70073 1.28324),(3.43534 0.441146),\
                      (2.09493 1.48836),(1.12031 2.2498),\
                      (0.358522 2.84496),(-0.705343 3.67612),\
                      (2.06005 1.27206),(2.3516 1.62191),(4 3.6),\
                      (5.09496 4.91395),(6.47672 4.09311),(4 4.74286),\
                      (2.54193 6.07595),(1.87562 6.68515),\
                      (1.43457 7.08839),(0.502294 7.64221),\
                      (0.601362 7.58336),(0.614728 3.49349),\
                      (0.619143 2.1426),(0.623165 0.911787),\
                      (0.623783 0.722855),(3.16036 -775.427199),\
                      (3.23365 -767.0972558),(1.01466 0.926246),\
                      (1.01183 1.90535),(1.01168 1.95744),\
                      (1.00439 4.47984),(0.91526 4.25422),\
                      (1.36441 2.90677),(1.8713 1.38609),\
                      (1.87531 1.37408),(0.0484053 -0.635122),\
                      (8.5655 2.85228),(5.26567 4.81254),(4 3.8),\
                      (1.4995 3.27036),(0.591231 3.43401),\
                      (-0.706503 3.66784),\
                      (-0.7654 8.88178e-16,-0.7654 0,5 3))"),
         from_wkt<ML>("MULTILINESTRING((1.87562 6.68515),(1.60494 6),\
                      (1.18124 4.9275),(1.00439 4.47984),(0.91526 4.25422),\
                      (0.729883 3.78498),(0.614728 3.49349),\
                      (0.591231 3.43401),(0.412281 2.98104),\
                      (0.358522 2.84496),(0.235887 2.53454),\
                      (0.10749 2.20953),(0.0954852 2.17914),\
                      (5 3),(0.0484053 -0.635122),(0.535994 0.677175),\
                      (0.623165 0.911787),(0.92255 1.71755),\
                      (1.01168 1.95744),(1.03214 2.01251),\
                      (1.12031 2.2498),(1.36441 2.90677),\
                      (1.38821 2.97083),(1.39905 3),(1.40063 3.00424),\
                      (1.4995 3.27036),(1.59592 3.52985),\
                      (1.99437 4.60225),(2.51371 6),(2.54193 6.07595),\
                      (4 6),(4 4.74286),(4 4.33333),(4 4.07748),(4 3.8),\
                      (4 3.8),(4 3.6),(4 3.5),(4 3.03179),(4 3),\
                      (4 2.9435),(4 2.82162),(4 2.47965),(4 1.72377),\
                      (4 1.38014),(3.43534 0.441146),(2.06005 1.27206),\
                      (1.88383 1.37852),(1.8713 1.38609),\
                      (1.01183 1.90535),(0.72677 2.07758),\
                      (0.619143 2.1426),(-0.532243 2.83823),\
                      (-0.789427 2.99361),(-0.756651 3.30964),\
                      (-0.706503 3.66784),(-0.705343 3.67612),\
                      (0.502294 7.64221),(0.601362 7.58336),\
                      (0.641699 7.5594),(1.43457 7.08839),\
                      (3.26673 6),(5.09496 4.91395),(5.26567 4.81254),\
                      (6.47672 4.09311),(8.18862 3.07616),\
                      (8.49103 2.89652),(8.5655 2.85228),\
                      (9.98265 0.00543606),(9.09826 -100.515944),\
                      (7.08745 -329.0674155),(5.06428 -559.024344),\
                      (3.23365 -767.0972558),(3.16036 -775.427199),\
                      (-0.7654 8.88178e-16,-0.7654 0,5 3))"),
#endif
          "mlmli21",
          1e-4
         );
}
#endif // BOOST_GEOMETRY_TEST_NO_DEGENERATE




BOOST_AUTO_TEST_CASE( test_intersection_ml_ml_spikes )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / MULTILINESTRING INTERSECTION" 
              << " (WITH SPIKES) ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_linestring_type ML;

    typedef test_intersection_of_geometries<ML, ML, ML> tester;

    // the following test cases concern linestrings with spikes

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0,5 0))"),
         "mlmli-spikes-01"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((9 0,1 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0))"),
         from_wkt<ML>("MULTILINESTRING((9 0,1 0,5 0))"),
         "mlmli-spikes-02"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0,2 0,8 0,3 0,7 0,4 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0,2 0,8 0,3 0,7 0,4 0,5 0))"),
         "mlmli-spikes-03"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,3 0,2 0,4 0,3 0,5 0,4 0,6 0,\
                      5 0,7 0,6 0,8 0,7 0,9 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,3 0,2 0,4 0,3 0,5 0,4 0,6 0,\
                      5 0,7 0,6 0,8 0,7 0,9 0))"),
         "mlmli-spikes-04"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,7 0),\
                      (9 1,9 0,9 2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,6 0),(7 0,8 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,7 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,6 0),(7 0,8 0),(9 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,7 0),(9 0))"),
#endif
         "mlmli-spikes-05"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,7 0),\
                      (9 0,9 2,9 1))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,6 0),(7 0,8 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,7 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,6 0),(7 0,8 0),(9 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,7 0),(9 0))"),
#endif
         "mlmli-spikes-05a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(9 0,6 0,8 0),\
                      (11 0,8 0,12 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0),(6 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(9 0,6 0,8 0),\
                      (10 0,8 0,10 0))"),
         "mlmli-spikes-06"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,0 0,-2 0),(11 0,10 0,12 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(10 0))"),
#endif
         "mlmli-spikes-07"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,-2 -2),(11 1,10 0,12 2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((0 0),(10 0))"),
#endif
         "mlmli-spikes-07a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(11 0,10 0,12 0),\
                      (7 5,7 0,8 0,6.5 0,8.5 0,8.5 5))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING((1 0,6 0),(6.5 0,8.5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,6.5 0,8.5 0))"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0,6 0),(6.5 0,8.5 0),(10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,6 0,5 0),(7 0,8 0,6.5 0,8.5 0),(10 0))"),
#endif
         "mlmli-spikes-08"
         );

    // now the first geometry has a spike
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,7 0,4 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,8 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,7 0,4 0,8 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,8 0))"),
         "mlmli-spikes-09"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,7 0,4 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(9 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(9 0,10 0))"),
         "mlmli-spikes-09a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,7 0,4 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0),(9 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0),(5 0,4 0,5 0),(9 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0),(9 0,10 0))"),
         "mlmli-spikes-09b"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,7 0,4 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0),(6 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0),(6 0,7 0,6 0),(5 0,4 0,5 0),\
                      (6 0,10 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0),(6 0,10 0))"),
         "mlmli-spikes-09c"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,8 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,8 0),(8 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,8 0))"),
         "mlmli-spikes-10"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,8 0,4 0),(2 0,9 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,9 0),(9 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,8 0,4 0),(2 0,9 0,5 0))"),
         "mlmli-spikes-11"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((11 1,10 0,12 2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((10 0))"),
#endif
         "mlmli-spikes-12"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((11 -1,10 0,12 -2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((10 0))"),
#endif
         "mlmli-spikes-12a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((11 0,10 0,12 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((10 0))"),
#endif
         "mlmli-spikes-13"
         );

    // the following three tests have been discussed with Adam
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((1 2,1 1,1 2))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((1 1))"),
#endif
         "mlmli-spikes-14"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 0,0 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,1 0,2 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((1 0))"),
#endif
         "mlmli-spikes-15"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 0,1 1,2 0))"),
#ifdef BOOST_GEOMETRY_INTERSECTION_DO_NOT_INCLUDE_ISOLATED_POINTS
         from_wkt<ML>("MULTILINESTRING()"),
#else
         from_wkt<ML>("MULTILINESTRING((1 1))"),
#endif
         "mlmli-spikes-16"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 1,1 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 1))"),
         from_wkt<ML>("MULTILINESTRING((2 1,1 1,2 1))"),
         "mlmli-spikes-17"
         );

    // test cases sent by Adam on the mailing list (equal slikes)
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 1,0 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,0 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,0 0))"),
         "mlmli-spikes-18"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 1,0 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,0 0,1 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,0 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,0 0,1 1))"),
         "mlmli-spikes-19"
         );
}
