// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_union_linear_linear
#endif

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#define BOOST_GEOMETRY_DEBUG_TURNS
#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_union_linear_linear.hpp"

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/algorithms/union.hpp>

typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::segment<point_type>                segment_type;
typedef bg::model::linestring<point_type>             linestring_type;
typedef bg::model::multi_linestring<linestring_type>  multi_linestring_type;



//===========================================================================
//===========================================================================
//===========================================================================


BOOST_AUTO_TEST_CASE( test_union_linestring_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** LINESTRING / LINESTRING UNION ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_union_of_geometries<L, L, ML> tester;

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 1,2 1,3 2)"),
         from_wkt<L>("LINESTRING(0 2,1 1,2 1,3 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,2 1,3 2),\
                      (0 2,1 1),(2 1,3 0))"),
         from_wkt<ML>("MULTILINESTRING((0 2,1 1,2 1,3 0),\
                      (0 0,1 1),(2 1,3 2))"),
         "llu00");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,5 0)"),
         from_wkt<L>("LINESTRING(3 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((3 0,4 0),(0 0,3 0),(4 0,5 0))"),
         "llu01");

    tester::apply
        (from_wkt<L>("LINESTRING(3 0,4 0)"),
         from_wkt<L>("LINESTRING(0 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((3 0,4 0),(0 0,3 0),(4 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,5 0))"),
         "llu01-1");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,6 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,4 0),(4 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((3 0,6 0),(0 0,3 0))"),
         "llu01-2");

    tester::apply
        (from_wkt<L>("LINESTRING(3 0,6 0)"),
         from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((3 0,6 0),(0 0,3 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,4 0),(4 0,6 0))"),
         "llu01-3");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,6 0)"),
         from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,4 0),(4 0,6 0))"),
         "llu01-4");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),\
                      (0 0,1 1,2 0,3 1,4 0),\
                      (5 0,6 1,7 -1,8 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0),\
                      (0 0,4 0),(5 0,20 0))"),
         "llu01-6");

    tester::apply
        (from_wkt<L>("LINESTRING(-20 0,20 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0)"),
         from_wkt<ML>("MULTILINESTRING((-20 0,20 0),\
                      (0 0,1 1,2 0,3 1,4 0),\
                      (5 0,6 1,7 -1,8 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0),\
                      (-20 0,4 0),(5 0,20 0))"),
         "llu01-7");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<L>("LINESTRING(2 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0),(0 0,2 0))"),
         "llu01-8");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0)"),
         from_wkt<L>("LINESTRING(4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,5 0))"),
         "llu01-10");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0)"),
         from_wkt<L>("LINESTRING(2 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(2 0,5 0))"),
         "llu01-11");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,4 0),(4 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((3 0,5 0),(0 0,1 0,3 0))"),
         "llu01-11a");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,4 0),(4 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((3 0,4 0,5 0),(0 0,1 0,3 0))"),
         "llu01-11b");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,6 0)"),
         from_wkt<L>("LINESTRING(2 0,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((2 0,4 0,5 0),(0 0,2 0),(5 0,6 0))"),
         "llu01-12");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,5 5,10 5,15 0)"),
         from_wkt<L>("LINESTRING(-1 6,0 5,15 5)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,5 5,10 5,15 0),\
                      (-1 6,0 5,5 5),(10 5,15 5))"),
         from_wkt<ML>("MULTILINESTRING((-1 6,0 5,15 5),\
                      (0 0,1 0,5 5),(10 5,15 0))"),
         "llu02");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0),\
                      (-1 0,0 0),(1 0,15 0),(20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,30 0),(-1 1,0 0),\
                      (1 0,5 5,10 5,15 0))"),
         "llu03");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,5 5,10 5,15 0,20 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,5 5,10 5,15 0,20 0),\
                      (-1 0,0 0),(1 0,15 0),(20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,30 0),(1 0,5 5,10 5,15 0))"),
         "llu04");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,25 1)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0,25 1),\
                      (-1 0,0 0),(1 0,15 0),(20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,30 0),(-1 1,0 0),\
                      (1 0,5 5,10 5,15 0),(20 0,25 1))"),
         "llu05");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0),\
                      (-1 0,0 0),(1 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,30 0),(-1 1,0 0),\
                      (1 0,5 5,10 5,15 0))"),
         "llu05-1");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0),\
                      (-1 0,0 0),(1 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,30 0),(-1 1,0 0),\
                      (1 0,5 5,10 5,15 0),(30 0,31 0))"),
         "llu06");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,25 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0),\
                      (-1 0,0 0),(1 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,25 0,30 0),(-1 1,0 0),\
                      (1 0,5 5,10 5,15 0),(30 0,31 0))"),
         "llu07");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0),\
                      (-1 0,0 0),(1 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,19 0,30 0),(-1 1,0 0),\
                      (1 0,5 5,10 5,15 0),(30 0,31 0))"),
         "llu08");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0,\
                      30 0,31 1),(-1 0,0 0),(1 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 0,30 0),(-1 1,0 0),\
                      (1 0,5 5,10 5,15 0),(30 0,31 1))"),
         "llu09");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,5 5,10 5,15 0,20 0,\
                      30 0,31 1),(-1 -1,0 0),(1 0,2 1,3 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,1 0,2 1,3 0,30 0),\
                      (-1 1,0 0),(1 0,5 5,10 5,15 0),(30 0,31 1))"),
         "llu10");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,20 0,\
                                 30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,4 0,5 5,10 5,15 0,20 0,\
                      30 0,31 1),(-1 -1,0 0),(2 0,2.5 1,3 0),(4 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0),\
                      (-1 1,0 0),(2 0,3 0),(4 0,5 5,10 5,15 0),(30 0,31 1))"),
         "llu11");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0),\
                      (-1 -1,0 0),(2 0,2.5 1,3 0),(4 0,15 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0),\
                      (-1 1,0 0),(2 0,3 0),(4 0,5 5,10 5,15 0),(30 0,31 0))"),
         "llu11-1");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0,3 1))"),
         "llu12");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<L>("LINESTRING(3 1,2 0,0 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((3 1,2 0,0 0))"),
         "llu12-1");

   tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 5,4 0)"),
         from_wkt<L>("LINESTRING(1 0,2 1,3 5,4 0,5 10)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 1,3 5,4 0),(4 0,5 10))"),
         from_wkt<ML>("MULTILINESTRING((1 0,2 1,3 5,4 0,5 10),(0 0,1 0))"),
         "llu13");

   tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 0,2.5 0,3 1)"),
         from_wkt<L>("LINESTRING(0 0,2 0,2.5 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0,2.5 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0,2.5 0,3 1))"),
         "llu14");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 5,4 0)"),
         from_wkt<L>("LINESTRING(1 0,2 1,3 5)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 1,3 5,4 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,2 1,3 5),(0 0,1 0),(3 5,4 0))"),
         "llu15");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 2)"),
         from_wkt<L>("LINESTRING(0.5 0,1 0,3 2,4 5)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 1,3 2),(3 2,4 5))"),
         from_wkt<ML>("MULTILINESTRING((0.5 0,1 0,3 2,4 5),(0 0,0.5 0))"),
         "llu16");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 2)"),
         from_wkt<L>("LINESTRING(4 5,3 2,1 0,0.5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 1,3 2),(4 5,3 2))"),
         from_wkt<ML>("MULTILINESTRING((4 5,3 2,1 0,0.5 0),(0 0,0.5 0))"),
         "llu16-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1,30 1)"),
         from_wkt<L>("LINESTRING(1 1,2 0,3 1,20 1,25 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1,30 1),(1 1,2 0,3 1,20 1))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,3 1,20 1,25 1),\
                      (0 0,10 0,20 1),(25 1,30 1))"),
         "llu17");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1,21 0,30 0)"),
         from_wkt<L>("LINESTRING(1 1,2 0,3 1,20 1,25 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1,21 0,30 0),\
                       (1 1,2 0,3 1,20 1,25 0))"),
         "llu18");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,4 0,5 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,4 0,5 1),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,4 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 1,4 0,4 1,20 1,5 0))"),
         from_wkt<ML>("MULTILINESTRING((5 1,4 0,4 1,20 1,5 0,1 0),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,4 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,4 0),(0 0,1 0),\
                      (5 0,30 0))"),
         "llu19a");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(4 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(4 0,4 1,20 1,5 0))"),
         from_wkt<ML>("MULTILINESTRING((4 0,4 1,20 1,5 0,1 0),(0 0,1 0),\
                      (5 0,30 0))"),
         "llu19a-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,4 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,4 0,5 0),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19b");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0,6 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,4 0),(5 0,6 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,4 0,5 0,6 1),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19c");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,3 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,4 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,4 0,3 0),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19d");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,3 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,4 0),(3 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,4 0,3 0,3 1),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19e");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0,5 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,4 0),(5 0,5 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,4 0,5 0,5 1),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19f");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,5 0,4 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 1,5 0),\
                      (4 0,4 1,20 1,5 0))"),
         from_wkt<ML>("MULTILINESTRING((5 1,5 0,4 0,4 1,20 1,5 0,1 0),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19f-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,5 0,5 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 0,20 1,4 1,5 0,5 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 0,20 1,4 1,5 0,5 1),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19g");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,5 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(5 1,5 0,4 1,20 1,5 0))"),
         from_wkt<ML>("MULTILINESTRING((5 1,5 0,4 1,20 1,5 0,1 0),\
                      (0 0,1 0),(5 0,30 0))"),
         "llu19g-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0,30 30,10 30,10 -10,15 0,40 0)"),
         from_wkt<L>("LINESTRING(5 5,10 0,10 30,20 0,25 0,25 25,50 0,35 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0,30 30,10 30,10 -10,15 0,40 0),\
                      (5 5,10 0),(10 30,20 0),(25 0,25 25,50 0,40 0))"),
         from_wkt<ML>("MULTILINESTRING((5 5,10 0,10 30,20 0,25 0,25 25,50 0,35 0),\
                      (0 0,20 0),(25 0,30 0,30 30,10 30),\
                      (10 0,10 -10,15 0,20 0),(25 0,35 0))"),
         "llu20");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0,30 30,10 30,10 -10,15 0,40 0)"),
         from_wkt<L>("LINESTRING(5 5,10 0,10 30,20 0,25 0,25 25,50 0,15 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0,30 30,10 30,10 -10,15 0,40 0),\
                      (5 5,10 0),(10 30,20 0),(25 0,25 25,50 0,40 0))"),
         from_wkt<ML>("MULTILINESTRING((5 5,10 0,10 30,20 0,25 0,25 25,50 0,15 0),\
                      (0 0,15 0),(30 0,30 30,10 30),(10 0,10 -10,15 0))"),
         "llu20a");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<L>("LINESTRING(2 2,5 -1,15 2,18 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0),\
                      (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((2 2,5 -1,15 2,18 0,20 0),\
                      (0 0,18 0),(20 0,30 0))"),
         "llu21"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(2 2,5 -1,15 2,18 0,20 0)"),
         from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((2 2,5 -1,15 2,18 0,20 0),\
                      (0 0,18 0),(20 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0),\
                      (2 2,5 -1,15 2,18 0))"),
         "llu21a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(-2 -2,-4 0,1 -8,-2 6,8 5,-7 -8,3 0,\
                     4 -1,-7 10,-4 10)"),
         from_wkt<L>("LINESTRING(-5 -4,3 0,4 -1,7 -4,2 -1,-4 -1,-2 6)"),
         from_wkt<ML>("MULTILINESTRING((-2 -2,-4 0,1 -8,-2 6,8 5,-7 -8,3 0,\
                     4 -1,-7 10,-4 10),(-5 -4,3 0),\
                     (4 -1,7 -4,2 -1,-4 -1,-2 6))"),
         from_wkt<ML>("MULTILINESTRING((-5 -4,3 0,4 -1,7 -4,2 -1,-4 -1,-2 6),\
                      (-2 -2,-4 0,1 -8,-2 6,8 5,-7 -8,3 0),\
                      (3 0,-7 10,-4 10))"),
         "llu22"
         );
}



BOOST_AUTO_TEST_CASE( test_union_linestring_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** LINESTRING / MULTILINESTRING UNION ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_union_of_geometries<L, ML, ML> tester;

    // disjoint linestrings
    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 3),(1 1,2 2,5 3))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 1,2 2,4 3),\
                      (1 1,2 2,5 3))"),
         "lmlu01"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 1,2 0),(1 1,3 0))"),
         "lmlu02"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 1,2 0),(1 1,3 0))"),
         "lmlu03"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 1,2 0))"),
         "lmlu04"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0),(-1 -1,1 0),(101 0,200 -1))"),
         "lmlu07"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,101 0),(-1 -1,0 0),\
                      (19 -1,20 0),(101 0,200 -1))"),
         "lmlu07a"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0),(-1 -1,0 0),\
                      (19 -1,20 0),(101 0,200 -1))"),
         "lmlu07b"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0),\
                      (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0),(0 1,1 1,2 0),\
                      (-1 -1,1 0),(101 0,200 -1))"),
         "lmlu08"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 0.5,3 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0.5,3 0,101 0),\
                      (0 1,1 1,2 0.5),(-1 -1,1 0,3 0),(101 0,200 -1))"),
         "lmlu09"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,1.5 0,2 0.5,3 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,1 0,2 0.5),\
                      (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,1.5 0,2 0.5,3 0,101 0),\
                      (0 1,1 1,1 0,2 0.5),(-1 -1,1 0),(1.5 0,3 0),\
                      (101 0,200 -1))"),
         "lmlu10"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (1 1,2 0,18 0,19 1),(2 1,3 0,17 0,18 1),\
                      (3 1,4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (1 1,2 0),(18 0,19 1),(2 1,3 0),(17 0,18 1),\
                      (3 1,4 0),(16 0,17 1))"),
         "lmlu12"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0,20 1),\
                      (2 0,18 0,19 1),(3 0,17 0,18 1),\
                      (4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(19 0,20 1),(18 0,19 1),\
                      (17 0,18 1),(16 0,17 1))"),
         "lmlu13"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),\
                      (19 0,20 1,19 1,18 0),(2 0,1 1,2 1,3 0),\
                      (17 0,18 1,17 1,16 0),(4 0,3 1))"),
         "lmlu14"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,4 2,6 0))"),
         "lmlu15"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (6 0,4 2,2 2))"),
         "lmlu15a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,5 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,4 2,5 0))"),
         "lmlu16"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,5 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (5 0,4 2,2 2))"),
         "lmlu16a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0))"),
         "lmlu17"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0,26 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0,26 2))"),
         "lmlu17a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,5 -1,15 2,18 0))"),
         "lmlu18"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0),(0 1,1 0),\
                      (19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         "lmlu18a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0),(0 1,1 0),\
                      (19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         "lmlu18b"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,25 0,26 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0),(0 1,1 0),\
                      (19 0,20 1),(2 2,5 -1,15 2,25 0))"),
         "lmlu18c"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,25 0,21 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0),(0 1,1 0),\
                      (19 0,20 1),(2 2,5 -1,15 2,25 0))"),
         "lmlu18d"
         );
}



BOOST_AUTO_TEST_CASE( test_union_multilinestring_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / LINESTRING UNION ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_union_of_geometries<ML, L, ML> tester;

    // disjoint linestrings
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<L>("LINESTRING(1 1,2 2,4 3)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 3),(0 0,10 0,20 1),(1 0,7 0))"),
         "mllu01"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<L>("LINESTRING(1 1,2 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(0 0,2 0),(4 0,10 0,20 1),\
                      (1 0,2 0),(4 0,7 0))"),
         "mllu02"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<L>("LINESTRING(-1 -1,1 0,101 0,200 -1)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1),(0 0,1 0))"),
         "mllu03"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<L>("LINESTRING(0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1),\
                       (0 0,1 0),(19 0,20 0))"),
         "mllu04"
         );
}







BOOST_AUTO_TEST_CASE( test_union_multilinestring_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / MULTILINESTRING UNION ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_linestring_type ML;

    typedef test_union_of_geometries<ML, ML, ML> tester;

    // disjoint linestrings
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 3),(1 1,2 2,5 3))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                      (1 1,2 2,4 3),(1 1,2 2,5 3))"),
         "mlmlu01"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                      (1 1,2 0),(1 1,3 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,4 0),\
                      (0 0,2 0),(4 0,10 0,20 1),(1 0,2 0),(4 0,7 0))"),
         "mlmlu02"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                      (1 1,2 0),(1 1,3 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,5 0),\
                      (0 0,2 0),(5 0,10 0,20 1),(1 0,2 0),(5 0,7 0))"),
         "mlmlu03"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                      (1 1,2 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(0 0,2 0),\
                      (4 0,10 0,20 1),(1 0,2 0),(4 0,7 0))"),
         "mlmlu04"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                      (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                      (10 20,15 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                      (10 10,20 10,30 20),(1 1,2 0),(10 20,15 10),\
                      (20 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                      (10 20,15 10,25 10,30 15),(0 0,2 0),(4 0,10 0,20 1),\
                      (1 0,2 0),(4 0,7 0),(10 10,15 10),(20 10,30 20))"),
         "mlmlu05"
        );


    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 10),(1 0,7 0),\
                      (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                      (-1 -1,0 0,9 0,11 10,12 10,13 3,14 4,15 5),\
                      (10 20,15 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 10),(1 0,7 0),\
                      (10 10,20 10,30 20),(1 1,2 0),\
                      (-1 -1,0 0),(9 0,11 10),(12 10,13 3),(10 20,15 10),\
                      (20 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                      (-1 -1,0 0,9 0,11 10,12 10,13 3,14 4,15 5),\
                      (10 20,15 10,25 10,30 15),(9 0,10 0,13 3),\
                      (15 5,20 10),(10 10,11 10),(12 10,15 10),(20 10,30 20))"),
         "mlmlu06"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                      (-1 -1,0 0,9 0,11 10,12 10,13 3,14 4,15 5),\
                      (10 20,15 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 10),(1 0,7 0),\
                      (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                      (-1 -1,0 0,9 0,11 10,12 10,13 3,14 4,15 5),\
                      (10 20,15 10,25 10,30 15),(9 0,10 0,13 3),\
                      (15 5,20 10),(10 10,11 10),(12 10,15 10),\
                      (20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 10),(1 0,7 0),\
                      (10 10,20 10,30 20),(1 1,2 0),(-1 -1,0 0),  \
                      (9 0,11 10),(12 10,13 3),(10 20,15 10),\
                      (20 10,25 10,30 15))"),
         "mlmlu06a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0),(-1 -1,1 0),(101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1),(0 0,1 0))"),
         "mlmlu07"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((-1 1,0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0,101 0),(-1 -1,0 0),\
                      (19 -1,20 0),(101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1),(-1 1,0 0))"),
         "mlmlu07a"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0),(-1 -1,0 0),\
                      (19 -1,20 0),(101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         "mlmlu07b"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0),\
                      (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,101 0),(0 1,1 1,2 0),\
                      (-1 -1,1 0),(101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0),\
                      (-1 -1,1 0,101 0,200 -1),(0 0,1 0))"),
         "mlmlu08"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0.5,3 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0.5),\
                      (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0.5,3 0,101 0),\
                      (0 1,1 1,2 0.5),(-1 -1,1 0,3 0),(101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0.5),\
                      (-1 -1,1 0,101 0,200 -1),(0 0,1 0,2 0.5,3 0))"),
         "mlmlu09"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 0,1.5 0,2 0.5,3 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,1 0,2 0.5),\
                      (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,1.5 0,2 0.5,3 0,101 0),\
                      (0 1,1 1,1 0,2 0.5),(-1 -1,1 0),(1.5 0,3 0),\
                      (101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,1 0,2 0.5),\
                      (-1 -1,1 0,101 0,200 -1),(0 0,1 0),(1.5 0,2 0.5,3 0))"),
         "mlmlu10"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 1,100 1,101 0),\
                      (0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1,3 0,4 0,5 1,6 1,\
                      7 0,8 0,9 1,10 1,11 0,12 0,13 1,14 1,15 0),\
                      (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1,100 1,101 0),\
                      (0 0,101 0),(1 0,1 1),(2 1,3 0),(4 0,5 1),(6 1,7 0),\
                      (8 0,9 1),(10 1,11 0),(12 0,13 1),(14 1,15 0),\
                      (-1 -1,1 0),(101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1,3 0,4 0,5 1,6 1,\
                      7 0,8 0,9 1,10 1,11 0,12 0,13 1,14 1,15 0),\
                      (-1 -1,1 0,101 0,200 -1),(0 0,1 1),(2 1,5 1),\
                      (6 1,9 1),(10 1,13 1),(14 1,100 1,101 0),(0 0,1 0))"),
         "mlmlu11"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (1 1,2 0,18 0,19 1),(2 1,3 0,17 0,18 1),\
                      (3 1,4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (1 1,2 0),(18 0,19 1),(2 1,3 0),(17 0,18 1),\
                      (3 1,4 0),(16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (1 1,2 0,18 0,19 1),(2 1,3 0,17 0,18 1),\
                      (3 1,4 0,16 0,17 1),(0 0,1 0),(19 0,20 0))"),
         "mlmlu12"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0,20 1),\
                      (2 0,18 0,19 1),(3 0,17 0,18 1),\
                      (4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(19 0,20 1),(18 0,19 1),\
                      (17 0,18 1),(16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0,20 1),\
                      (2 0,18 0,19 1),(3 0,17 0,18 1),\
                      (4 0,16 0,17 1),(0 0,1 0),(19 0,20 0))"),
         "mlmlu13"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),\
                      (19 0,20 1,19 1,18 0),(2 0,1 1,2 1,3 0),\
                      (17 0,18 1,17 1,16 0),(4 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1),\
                       (0 0,1 0),(19 0,20 0))"),
         "mlmlu14"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                       (2 2,4 2,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,6 0),(0 0,1 0),(19 0,20 0))"),
         "mlmlu15"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (6 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (6 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (6 0,4 2,2 2),(0 0,1 0),(19 0,20 0))"),
         "mlmlu15a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,4 2,5 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,4 2,5 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,4 2,5 0,6 0),(0 0,1 0),(19 0,20 0))"),
         "mlmlu16"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (6 0,5 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(0 1,1 0),(19 0,20 1),\
                      (5 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (6 0,5 0,4 2,2 2),(0 0,1 0),(19 0,20 0))"),
         "mlmlu16a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0),(0 0,1 0),(19 0,30 0))"),
         "mlmlu17"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0,26 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0,26 2))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,4 0,5 2,20 2,25 0,26 2),(0 0,1 0),(19 0,30 0))"),
         "mlmlu17a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,30 0),(0 1,1 0),(19 0,20 1),\
                      (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,5 -1,15 2,18 0),(0 0,1 0),(19 0,30 0))"),
         "mlmlu18"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0),(0 1,1 0),\
                      (19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (2 2,5 -1,15 2,18 0),(0 0,1 0),(19 0,30 0))"),
         "mlmlu18a"
         );
}
