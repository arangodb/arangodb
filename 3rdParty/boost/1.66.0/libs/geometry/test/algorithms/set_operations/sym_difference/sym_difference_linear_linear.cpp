// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015 Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_sym_difference_linear_linear
#endif

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#define BOOST_GEOMETRY_DEBUG_TURNS
#define BOOST_GEOMETRY_DEBUG_SEGMENT_IDENTIFIER
#endif

#include <boost/test/included/unit_test.hpp>

#include "test_sym_difference_linear_linear.hpp"

#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/algorithms/sym_difference.hpp>

typedef bg::model::point<double,2,bg::cs::cartesian>  point_type;
typedef bg::model::segment<point_type>                segment_type;
typedef bg::model::linestring<point_type>             linestring_type;
typedef bg::model::multi_linestring<linestring_type>  multi_linestring_type;



//===========================================================================
//===========================================================================
//===========================================================================


BOOST_AUTO_TEST_CASE( test_sym_difference_linestring_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** LINESTRING / LINESTRING SYMMETRIC DIFFERENCE ***" << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_sym_difference_of_geometries<L, L, ML> tester;

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 1,2 1,3 2)"),
         from_wkt<L>("LINESTRING(0 2,1 1,2 1,3 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1),(2 1,3 2),\
                      (0 2,1 1),(2 1,3 0))"),
         "llsdf00");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,5 0)"),
         from_wkt<L>("LINESTRING(3 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,3 0),(4 0,5 0))"),
         "llsdf01");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,6 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,3 0),(4 0,6 0))"),
         "llsdf01-1");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,6 0)"),
         from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((4 0,6 0))"),
         "llsdf01-2");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,4 0),(5 0,20 0),\
                      (0 0,1 1,2 0,3 1,4 0),(5 0,6 1,7 -1,8 0))"),
         "llsdf01-3");

    tester::apply
        (from_wkt<L>("LINESTRING(-20 0,20 0)"),
         from_wkt<L>("LINESTRING(0 0,1 1,2 0,3 1,4 0,5 0,6 1,7 -1,8 0)"),
         from_wkt<ML>("MULTILINESTRING((-20 0,4 0),(5 0,20 0),\
                      (0 0,1 1,2 0,3 1,4 0),(5 0,6 1,7 -1,8 0))"),
         "llsdf01-4");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,4 0)"),
         from_wkt<L>("LINESTRING(2 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0))"),
         "llsdf01-5");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0)"),
         from_wkt<L>("LINESTRING(4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,5 0))"),
         "llsdf01-6");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0)"),
         from_wkt<L>("LINESTRING(2 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(2 0,5 0))"),
         "llsdf01-7");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,3 0),(4 0,5 0))"),
         "llsdf01-8");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,4 0)"),
         from_wkt<L>("LINESTRING(3 0,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,3 0),(4 0,5 0))"),
         "llsdf01-9");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,6 0)"),
         from_wkt<L>("LINESTRING(2 0,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(5 0,6 0))"),
         "llsdf01-10");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,5 5,10 5,15 0)"),
         from_wkt<L>("LINESTRING(-1 6,0 5,15 5)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,5 5),(10 5,15 0),\
                      (-1 6,0 5,5 5),(10 5,15 5))"),
         "llsdf02");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (-1 0,0 0),(1 0,15 0),(20 0,30 0))"),
         "llsdf03");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,5 5,10 5,15 0,20 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,5 5,10 5,15 0),\
                      (-1 0,0 0),(1 0,15 0),(20 0,30 0))"),
         "llsdf04");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,25 1)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (20 0,25 1),(-1 0,0 0),(1 0,15 0),(20 0,30 0))"),
         "llsdf05");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (-1 0,0 0),(1 0,15 0))"),
         "llsdf05-1");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (30 0,31 0),(-1 0,0 0),(1 0,15 0))"),
         "llsdf06");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,25 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (30 0,31 0),(-1 0,0 0),(1 0,15 0))"),
         "llsdf07");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (30 0,31 0),(-1 0,0 0),(1 0,15 0))"),
         "llsdf08");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (30 0,31 1),(-1 0,0 0),(1 0,15 0))"),
         "llsdf09");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,5 5,10 5,15 0,20 0,30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(1 0,5 5,10 5,15 0),\
                      (30 0,31 1),(-1 -1,0 0),(1 0,2 1,3 0,15 0))"),
         "llsdf10");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,20 0,\
                                 30 0,31 1)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(2 0,3 0),\
                      (4 0,5 5,10 5,15 0),(30 0,31 1),(-1 -1,0 0),\
                      (2 0,2.5 1,3 0),(4 0,15 0))"),
         "llsdf11");

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,1 0,4 0,5 5,10 5,15 0,31 0)"),
         from_wkt<L>("LINESTRING(-1 -1,0 0,1 0,2 0,2.5 1,3 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(2 0,3 0),\
                      (4 0,5 5,10 5,15 0),(30 0,31 0),(-1 -1,0 0),\
                      (2 0,2.5 1,3 0),(4 0,15 0))"),
         "llsdf11-1");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING()"),
         "llsdf12");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,2 0,3 1)"),
         from_wkt<L>("LINESTRING(3 1,2 0,0 0)"),
         from_wkt<ML>("MULTILINESTRING()"),
         "llsdf12-1");

   tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 5,4 0)"),
         from_wkt<L>("LINESTRING(1 0,2 1,3 5,4 0,5 10)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(4 0,5 10))"),
         "llsdf13");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 0,2.5 0,3 1)"),
         from_wkt<L>("LINESTRING(0 0,2 0,2.5 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING()"),
         "llsdf14");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 5,4 0)"),
         from_wkt<L>("LINESTRING(1 0,2 1,3 5)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(3 5,4 0))"),
         "llsdf15");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 2)"),
         from_wkt<L>("LINESTRING(0.5 0,1 0,3 2,4 5)"),
         from_wkt<ML>("MULTILINESTRING((0 0,0.5 0),(3 2,4 5))"),
         "llsdf16");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 1,3 2)"),
         from_wkt<L>("LINESTRING(4 5,3 2,1 0,0.5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,0.5 0),(4 5,3 2))"),
         "llsdf16-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1,30 1)"),
         from_wkt<L>("LINESTRING(1 1,2 0,3 1,20 1,25 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(25 1,30 1),\
                      (1 1,2 0,3 1,20 1))"),
         "llsdf17");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1,21 0,30 0)"),
         from_wkt<L>("LINESTRING(1 1,2 0,3 1,20 1,25 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1,21 0,30 0),\
                      (1 1,2 0,3 1,20 1,25 0))"),
         "llsdf18");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,4 0,5 1))"),
         "llsdf19");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,4 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 1,4 0,4 1,20 1,5 0))"),
         "llsdf19-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,4 0))"),
         "llsdf19a");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(4 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (4 0,4 1,20 1,5 0))"),
         "llsdf19a-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,4 0))"),
         "llsdf19b");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0,6 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,4 0),(5 0,6 1))"),
         "llsdf19c");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,3 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,4 0))"),
         "llsdf19d");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,3 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,4 0),(3 0,3 1))"),
         "llsdf19e");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,4 0,5 0,5 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,4 0),(5 0,5 1))"),
         "llsdf19f");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,5 0,4 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 1,5 0),(4 0,4 1,20 1,5 0))"),
         "llsdf19f-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(1 0,5 0,20 1,4 1,5 0,5 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 0,20 1,4 1,5 0,5 1))"),
         "llsdf19g");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<L>("LINESTRING(5 1,5 0,4 1,20 1,5 0,1 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(5 0,30 0),\
                      (5 1,5 0,4 1,20 1,5 0))"),
         "llsdf19g-r");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0,30 30,10 30,10 -10,15 0,40 0)"),
         from_wkt<L>("LINESTRING(5 5,10 0,10 30,20 0,25 0,25 25,50 0,35 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,20 0),(25 0,30 0,30 30,10 30),\
                      (10 0,10 -10,15 0,20 0),(25 0,35 0),\
                      (5 5,10 0),(10 30,20 0),(25 0,25 25,50 0,40 0))"),
         "llsdf20");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0,30 30,10 30,10 -10,15 0,40 0)"),
         from_wkt<L>("LINESTRING(5 5,10 0,10 30,20 0,25 0,25 25,50 0,15 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,15 0),(30 0,30 30,10 30),\
                      (10 0,10 -10,15 0),(5 5,10 0),(10 30,20 0),\
                      (25 0,25 25,50 0,40 0))"),
         "llsdf20a");

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<L>("LINESTRING(2 2,5 -1,15 2,18 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,18 0),(20 0,30 0),\
                      (2 2,5 -1,15 2,18 0))"),
         "llsdf21"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 -3,5 4,6 6,-3 2,-3 0,-3 -10,9 -2,\
                     9 5,5 -5,-4 -8,9 0)"),
         from_wkt<L>("LINESTRING(-3 6,-3 0,-3 5,2 -3,-6 10,5 0,\
                     2 8,-6 1,10 -6)"),
         from_wkt<ML>("MULTILINESTRING((0 -3,5 4,6 6,-3 2),\
                      (-3 0,-3 -10,9 -2,9 5,5 -5,-4 -8,9 0),(-3 6,-3 2),\
                      (-3 2,-3 5,2 -3,-6 10,5 0,2 8,-6 1,10 -6))"),
         "llsdf22"
         );
}



BOOST_AUTO_TEST_CASE( test_sym_difference_linestring_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** LINESTRING / MULTILINESTRING SYMMETRIC DIFFERENCE ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_sym_difference_of_geometries<L, ML, ML> tester;

    // disjoint linestrings
    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 3),(1 1,2 2,5 3))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),\
                       (1 1,2 2,4 3),(1 1,2 2,5 3))"),
         "lmlsdf01"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,10 0,20 1),\
                      (1 1,2 0),(1 1,3 0))"),
         "lmlsdf02"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(5 0,10 0,20 1),\
                      (1 1,2 0),(1 1,3 0))"),
         "lmlsdf03"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,10 0,20 1)"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,10 0,20 1),\
                      (1 1,2 0))"),
         "lmlsdf04"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(-1 -1,1 0),(101 0,200 -1))"),
         "lmlsdf07"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(-1 1,0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(-1 -1,0 0),\
                      (19 -1,20 0),(101 0,200 -1))"),
         "lmlsdf07a"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0),(19 -1,20 0),\
                      (101 0,200 -1))"),
         "lmlsdf07b"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(0 1,1 1,2 0),\
                      (-1 -1,1 0),(101 0,200 -1))"),
         "lmlsdf08"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,2 0.5,3 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0.5,3 0),\
                      (0 1,1 1,2 0.5),(-1 -1,1 0,3 0),(101 0,200 -1))"),
         "lmlsdf09"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,1 0,1.5 0,2 0.5,3 0,101 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,1 0,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(1.5 0,2 0.5,3 0),\
                      (0 1,1 1,1 0,2 0.5),(-1 -1,1 0),(1.5 0,3 0),\
                      (101 0,200 -1))"),
         "lmlsdf10"
        );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (1 1,2 0,18 0,19 1),(2 1,3 0,17 0,18 1),\
                      (3 1,4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(1 1,2 0),(18 0,19 1),\
                      (2 1,3 0),(17 0,18 1),(3 1,4 0),(16 0,17 1))"),
         "lmlsdf12"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0,20 1),\
                      (2 0,18 0,19 1),(3 0,17 0,18 1),\
                      (4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (19 0,20 1),(18 0,19 1),(17 0,18 1),(16 0,17 1))"),
         "lmlsdf13"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(2 0,18 0),(3 0,17 0),\
                      (4 0,16 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0))"),
         "lmlsdf13a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1,19 1,18 0),(2 0,1 1,2 1,3 0),\
                      (17 0,18 1,17 1,16 0),(4 0,3 1))"),
         "lmlsdf14"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 2,6 0))"),
         "lmlsdf15"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(6 0,4 2,2 2))"),
         "lmlsdf15a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,5 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 2,5 0))"),
         "lmlsdf16"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,20 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,5 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(5 0,4 2,2 2))"),
         "lmlsdf16a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 0,5 2,20 2,25 0))"),
         "lmlsdf17"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0,26 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 0,5 2,20 2,25 0,26 2))"),
         "lmlsdf17a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         "lmlsdf18"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         "lmlsdf18a"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(20 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         "lmlsdf18b"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,25 0,26 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,25 0),(26 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,5 -1,15 2,25 0))"),
         "lmlsdf18c"
         );

    tester::apply
        (from_wkt<L>("LINESTRING(0 0,18 0,19 0,30 0)"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,25 0,21 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,21 0),(25 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,5 -1,15 2,25 0))"),
         "lmlsdf18d"
         );

#ifdef GEOMETRY_TEST_INCLUDE_FAILING_TESTS
    tester::apply
        (from_wkt<L>("LINESTRING(1 5,4 2,1 -1.1,1 6,1 189.7654,2 5,-0.7654 3)"),
         from_wkt<ML>("MULTILINESTRING((2 5,-0.7654 2),\
                      (-1032.34324 4,1 5,9 7,3 9,0.2 5,1 -0.3),\
                      (1 0.9,1 6,1 -0.6,2.232432 -0.7654,0.9 3,1 5,\
                      -0.7654 9,3 0.1,9 0,-6 8,-0.7 8,0 1,-1032.34324 0))"),
         from_wkt<ML>("MULTILINESTRING((2 5,-0.7654 2),\
                      (-1032.34324 4,1 5,9 7,3 9,0.2 5,1 -0.3),\
                      (1 -0.6,2.232432 -0.7654,0.9 3,1 5,-0.7654 9,\
                      3 0.1,9 0,-6 8,-0.7 8,0 1,-1032.34324 0),\
                      (1 5,4 2,1 -1.1,1 -0.6),\
                      (1 6,1 189.7654,2 5,-0.7654 3))"),
         "lmlsdf19",
         1e-10
         );
#endif
}




BOOST_AUTO_TEST_CASE( test_sym_difference_multilinestring_linestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / LINESTRING SYMMETRIC DIFFERENCE ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef linestring_type L;
    typedef multi_linestring_type ML;

    typedef test_sym_difference_of_geometries<ML, L, ML> tester;

    // disjoint linestrings
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<L>("LINESTRING(1 1,2 2,4 3)"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                                       (1 1,2 2,4 3))"),
         "mllsdf01"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<L>("LINESTRING(1 1,2 0,4 0)"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,10 0,20 1),\
                      (1 0,2 0),(4 0,7 0),(1 1,2 0))"),
         "mllsdf02"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<L>("LINESTRING(-1 -1,1 0,101 0,200 -1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(-1 -1,1 0),(101 0,200 -1))"),
         "mllsdf03"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<L>("LINESTRING(0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1)"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1,19 1,18 0),(2 0,1 1,2 1,3 0),\
                      (17 0,18 1,17 1,16 0),(4 0,3 1))"),
         "mllsdf04"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 -3,5 4,6 6,-3 2,-3 0,\
                      -3 -10,9 -2,9 5,5 -5,-4 -8,9 0),\
                      (-7 9,-4 -9,-5 -10,5 3),\
                      (-8 -3,-6 6,-9 0,-4 -3,-1 -10),\
                      (0 1,7 -1,-2 3,-7 1),\
                      (-5 -9,-4 -10,7 -10,0 -6,1 6,2 -1,1 5,-5 -5))"),
         from_wkt<L>("LINESTRING(-3 6,-3 0,-3 5,2 -3,-6 10,5 0,2 8,\
                     -6 1,10 -6)"),
         from_wkt<ML>("MULTILINESTRING((0 -3,5 4,6 6,-3 2),\
                      (-3 0,-3 -10,9 -2,9 5,5 -5,-4 -8,9 0),\
                      (-7 9,-4 -9,-5 -10,5 3),\
                      (-8 -3,-6 6,-9 0,-4 -3,-1 -10),\
                      (0 1,7 -1,-2 3,-7 1),\
                      (-5 -9,-4 -10,7 -10,0 -6,1 6,2 -1,1 5,-5 -5),\
                      (-3 6,-3 2),(-3 2,-3 5,2 -3,-6 10,5 0,2 8,-6 1,10 -6))"),
         "mllsdf05"
         );
}






BOOST_AUTO_TEST_CASE( test_sym_difference_multilinestring_multilinestring )
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl << std::endl << std::endl;
    std::cout << "*** MULTILINESTRING / MULTILINESTRING SYMMETRIC DIFFERENCE ***"
              << std::endl;
    std::cout << std::endl;
#endif

    typedef multi_linestring_type ML;

    typedef test_sym_difference_of_geometries<ML, ML, ML> tester;

    // disjoint linestrings
    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 2,4 3),(1 1,2 2,5 3))"),
         from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                      (1 1,2 2,4 3),(1 1,2 2,5 3))"),
         "mlmlsdf01"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,10 0,20 1),\
                      (1 0,2 0),(4 0,7 0),(1 1,2 0),(1 1,3 0))"),
         "mlmlsdf02"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),(1 1,3 0,5 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(5 0,10 0,20 1),\
                      (1 0,2 0),(5 0,7 0),(1 1,2 0),(1 1,3 0))"),
         "mlmlsdf03"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,10 0,20 1),\
                      (1 0,2 0),(4 0,7 0),(1 1,2 0))"),
         "mlmlsdf04"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                       (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                       (10 20,15 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((0 0,2 0),(4 0,10 0,20 1),\
                      (1 0,2 0),(4 0,7 0),(10 10,15 10),(20 10,30 20),\
                      (1 1,2 0),(10 20,15 10),(20 10,25 10,30 15))"),
         "mlmlsdf05"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 1),(1 0,7 0),\
                       (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                       (-1 -1,0 0,9 0,11 10,12 10,13 0.3,14 0.4,15 0.5),\
                       (10 20,15 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((9 0,10 0,13 0.3),(15 0.5,20 1),\
                      (10 10,11 10),(12 10,15 10),(20 10,30 20),\
                      (1 1,2 0),(-1 -1,0 0),(9 0,11 10),(12 10,13 0.3),\
                      (10 20,15 10),(20 10,25 10,30 15))"),
         "mlmlsdf06",
         4.0 * std::numeric_limits<double>::epsilon()
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,10 0,20 10),(1 0,7 0),\
                       (10 10,20 10,30 20))"),
         from_wkt<ML>("MULTILINESTRING((1 1,2 0,4 0),\
                       (-1 -1,0 0,9 0,11 10,12 10,13 3,14 4,15 5),\
                       (10 20,15 10,25 10,30 15))"),
         from_wkt<ML>("MULTILINESTRING((9 0,10 0,13 3),(15 5,20 10),\
                      (10 10,11 10),(12 10,15 10),(20 10,30 20),\
                      (1 1,2 0),(-1 -1,0 0),(9 0,11 10),(12 10,13 3),\
                      (10 20,15 10),(20 10,25 10,30 15))"),
         "mlmlsdf06a"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(-1 -1,1 0),\
                      (101 0,200 -1))"),
         "mlmlsdf07"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((-1 1,0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 1,0 0),(-1 -1,0 0),\
                      (19 -1,20 0),(101 0,200 -1))"),
         "mlmlsdf07a"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0,50 0),\
                      (19 -1,20 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((-1 -1,0 0),(19 -1,20 0),\
                      (101 0,200 -1))"),
         "mlmlsdf07b"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(0 1,1 1,2 0),\
                      (-1 -1,1 0),(101 0,200 -1))"),
         "mlmlsdf08"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0.5,3 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0,2 0.5,3 0),(0 1,1 1,2 0.5),\
                      (-1 -1,1 0,3 0),(101 0,200 -1))"),
         "mlmlsdf09"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 0,1.5 0,2 0.5,3 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 1,1 0,2 0.5),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(1.5 0,2 0.5,3 0),\
                      (0 1,1 1,1 0,2 0.5),(-1 -1,1 0),(1.5 0,3 0),\
                      (101 0,200 -1))"),
         "mlmlsdf10"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,1 1,100 1,101 0),\
                       (0 0,101 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,1 1,2 1,3 0,4 0,5 1,6 1,\
                       7 0,8 0,9 1,10 1,11 0,12 0,13 1,14 1,15 0),\
                       (-1 -1,1 0,101 0,200 -1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 1),(2 1,5 1),(6 1,9 1),\
                       (10 1,13 1),(14 1,100 1,101 0),(0 0,1 0),\
                       (1 0,1 1),(2 1,3 0),(4 0,5 1),(6 1,7 0),\
                       (8 0,9 1),(10 1,11 0),(12 0,13 1),(14 1,15 0),\
                       (-1 -1,1 0),(101 0,200 -1))"),
         "mlmlsdf11"
        );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                      (1 1,2 0,18 0,19 1),(2 1,3 0,17 0,18 1),\
                      (3 1,4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(1 1,2 0),(18 0,19 1),\
                      (2 1,3 0),(17 0,18 1),(3 1,4 0),(16 0,17 1))"),
         "mlmlsdf12"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0,20 1),\
                      (2 0,18 0,19 1),(3 0,17 0,18 1),\
                      (4 0,16 0,17 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (19 0,20 1),(18 0,19 1),(17 0,18 1),(16 0,17 1))"),
         "mlmlsdf13"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((1 0,19 0),(2 0,18 0),(3 0,17 0),\
                      (4 0,16 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0))"),
         "mlmlsdf13a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1,19 1,18 0,2 0,\
                       1 1,2 1,3 0,17 0,18 1,17 1,16 0,4 0,3 1))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1,19 1,18 0),(2 0,1 1,2 1,3 0),\
                      (17 0,18 1,17 1,16 0),(4 0,3 1))"),
         "mlmlsdf14"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 2,6 0))"),
         "mlmlsdf15"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(6 0,4 2,2 2))"),
         "mlmlsdf15a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 2,5 0,6 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 2,5 0))"),
         "mlmlsdf16"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,20 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (6 0,5 0,4 2,2 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,20 0),\
                      (0 1,1 0),(19 0,20 1),(5 0,4 2,2 2))"),
         "mlmlsdf16a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 0,5 2,20 2,25 0))"),
         "mlmlsdf17"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,4 0,5 2,20 2,25 0,26 2))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,4 0,5 2,20 2,25 0,26 2))"),
         "mlmlsdf17a"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         "mlmlsdf18"
         );

    tester::apply
        (from_wkt<ML>("MULTILINESTRING((0 0,18 0,19 0,30 0))"),
         from_wkt<ML>("MULTILINESTRING((0 1,1 0,19 0,20 1),\
                       (2 2,5 -1,15 2,18 0))"),
         from_wkt<ML>("MULTILINESTRING((0 0,1 0),(19 0,30 0),\
                      (0 1,1 0),(19 0,20 1),(2 2,5 -1,15 2,18 0))"),
         "mlmlsdf18a"
         );
}
