// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2015, 2017.
// Modifications copyright (c) 2015-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/geometries/point_xy.hpp>

#include "test_difference.hpp"
#include <algorithms/test_overlay.hpp>
#include <algorithms/overlay/overlay_cases.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>


#ifdef HAVE_TTMATH
#  include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif

template <typename CoordinateType>
void test_ticket_10835(std::string const& wkt_out1, std::string const& wkt_out2)
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;
    typedef bg::model::linestring<point_type> linestring_type;
    typedef bg::model::multi_linestring<linestring_type> multilinestring_type;
    typedef bg::model::polygon
        <
            point_type, /*ClockWise*/false, /*Closed*/false
        > polygon_type;

    multilinestring_type multilinestring;
    bg::read_wkt(ticket_10835[0], multilinestring);
    polygon_type polygon1;
    bg::read_wkt(ticket_10835[1], polygon1);
    polygon_type polygon2;
    bg::read_wkt(ticket_10835[2], polygon2);

    multilinestring_type multilinestringOut1;
    bg::difference(multilinestring, polygon1, multilinestringOut1);
    std::stringstream stream;
    stream << bg::wkt(multilinestringOut1);

    BOOST_CHECK_EQUAL(stream.str(), wkt_out1);

    multilinestring_type multilinestringOut2;
    bg::difference(multilinestringOut1, polygon2, multilinestringOut2);
    stream.str("");
    stream.clear();
    stream << bg::wkt(multilinestringOut2);

    BOOST_CHECK_EQUAL(stream.str(), wkt_out2);
}


template <typename Polygon, typename LineString>
void test_areal_linear()
{
    typedef typename bg::point_type<Polygon>::type point;
    typedef typename bg::coordinate_type<point>::type ct;

    std::string const poly_simplex = "POLYGON((1 1,1 3,3 3,3 1,1 1))";
    test_one_lp<LineString, LineString, Polygon>("simplex", "LINESTRING(0 2,4 2)", poly_simplex, 2, 4, 2.0);
    test_one_lp<LineString, LineString, Polygon>("case2",  "LINESTRING(0 1,4 3)", poly_simplex, 2, 4, sqrt(5.0));
    test_one_lp<LineString, LineString, Polygon>("case3", "LINESTRING(0 1,1 2,3 2,4 3,6 3,7 4)", "POLYGON((2 0,2 5,5 5,5 0,2 0))", 2, 6, 2.0 + 2.0 * sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case4", "LINESTRING(1 1,3 2,1 3)", "POLYGON((0 0,0 4,2 4,2 0,0 0))", 1, 3, sqrt(5.0));

    test_one_lp<LineString, LineString, Polygon>("case5", "LINESTRING(0 1,3 4)", poly_simplex, 2, 4, 2.0 * sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case6", "LINESTRING(1 1,10 3)", "POLYGON((2 0,2 4,3 4,3 1,4 1,4 3,5 3,5 1,6 1,6 3,7 3,7 1,8 1,8 3,9 3,9 0,2 0))", 5, 10,
            // Pieces are 1 x 2/9:
            5.0 * sqrt(1.0 + 4.0/81.0));


    test_one_lp<LineString, LineString, Polygon>("case7", "LINESTRING(1.5 1.5,2.5 2.5)", poly_simplex, 0, 0, 0.0);
    test_one_lp<LineString, LineString, Polygon>("case8", "LINESTRING(1 0,2 0)", poly_simplex, 1, 2, 1.0);

    std::string const poly_9 = "POLYGON((1 1,1 4,4 4,4 1,1 1))";
    test_one_lp<LineString, LineString, Polygon>("case9", "LINESTRING(0 1,1 2,2 2)", poly_9, 1, 2, sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case10", "LINESTRING(0 1,1 2,0 2)", poly_9, 1, 3, 1.0 + sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case11", "LINESTRING(2 2,4 2,3 3)", poly_9, 0, 0, 0.0);
    test_one_lp<LineString, LineString, Polygon>("case12", "LINESTRING(2 3,4 4,5 6)", poly_9, 1, 2, sqrt(5.0));

    test_one_lp<LineString, LineString, Polygon>("case13", "LINESTRING(3 2,4 4,2 3)", poly_9, 0, 0, 0.0);
    test_one_lp<LineString, LineString, Polygon>("case14", "LINESTRING(5 6,4 4,6 5)", poly_9, 1, 3, 2.0 * sqrt(5.0));

    test_one_lp<LineString, LineString, Polygon>("case15", "LINESTRING(0 2,1 2,1 3,0 3)", poly_9, 2, 4, 2.0);
    test_one_lp<LineString, LineString, Polygon>("case16", "LINESTRING(2 2,1 2,1 3,2 3)", poly_9, 0, 0, 0.0);

    std::string const angly = "LINESTRING(2 2,2 1,4 1,4 2,5 2,5 3,4 3,4 4,5 4,3 6,3 5,2 5,2 6,0 4)";
    test_one_lp<LineString, LineString, Polygon>("case17", angly, "POLYGON((1 1,1 5,4 5,4 1,1 1))", 3, 11, 6.0 + 4.0 * sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case18", angly, "POLYGON((1 1,1 5,5 5,5 1,1 1))", 2, 6, 2.0 + 3.0 * sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case19", "LINESTRING(1 2,1 3,0 3)", poly_9, 1, 2, 1.0);
    test_one_lp<LineString, LineString, Polygon>("case20", "LINESTRING(1 2,1 3,2 3)", poly_9, 0, 0, 0.0);

    // PROPERTIES CHANGED BY switch_to_integer
    // TODO test_one_lp<LineString, LineString, Polygon>("case21", "LINESTRING(1 2,1 4,4 4,4 1,2 1,2 2)", poly_9, 0, 0, 0.0);

    // More collinear (opposite) cases
    test_one_lp<LineString, LineString, Polygon>("case22", "LINESTRING(4 1,4 4,7 4)", poly_9, 1, 2, 3.0);
    test_one_lp<LineString, LineString, Polygon>("case23", "LINESTRING(4 0,4 4,7 4)", poly_9, 2, 4, 4.0);
    test_one_lp<LineString, LineString, Polygon>("case24", "LINESTRING(4 1,4 5,7 5)", poly_9, 1, 3, 4.0);
    test_one_lp<LineString, LineString, Polygon>("case25", "LINESTRING(4 0,4 5,7 5)", poly_9, 2, 5, 5.0);
    test_one_lp<LineString, LineString, Polygon>("case26", "LINESTRING(4 0,4 3,4 5,7 5)", poly_9, 2, 5, 5.0);
    test_one_lp<LineString, LineString, Polygon>("case27", "LINESTRING(4 4,4 5,5 5)", poly_9, 1, 3, 2.0);

    if (BOOST_GEOMETRY_CONDITION( (! boost::is_same<ct, float>::value)) )
    {
        // Fails for float
        test_one_lp<LineString, LineString, Polygon>("case28",
            "LINESTRING(-1.3 0,-15 0,-1.3 0)",
            "POLYGON((2 3,-9 -7,12 -13,2 3))",
             1, 3, 27.4);
    }

    test_one_lp<LineString, LineString, Polygon>("case29",
        "LINESTRING(5 5,-10 5,5 5)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         1, 3, 20);

    test_one_lp<LineString, LineString, Polygon>("case29a",
        "LINESTRING(1 1,5 5,-10 5,5 5,6 6)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         1, 3, 20);

    test_one_lp<LineString, LineString, Polygon>("case30",
        "LINESTRING(-10 5,5 5,-10 5)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         2, 4, 20);

    test_one_lp<LineString, LineString, Polygon>("case30a",
        "LINESTRING(-20 10,-10 5,5 5,-10 5,-20 -10)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         2, 6, 49.208096);

    test_one_lp<LineString, LineString, Polygon>("case31",
        "LINESTRING(0 5,5 5,0 5)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         0, 0, 0);

    test_one_lp<LineString, LineString, Polygon>("case31",
        "LINESTRING(0 5,5 5,1 1,9 1,5 5,0 5)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         0, 0, 0);

    test_one_lp<LineString, LineString, Polygon>("case32",
        "LINESTRING(5 5,0 5,5 5)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         0, 0, 0);

    test_one_lp<LineString, LineString, Polygon>("case32a",
        "LINESTRING(-10 10,5 5,0 5,5 5,20 10)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         2, 4, 21.081851);

    test_one_lp<LineString, LineString, Polygon>("case33",
        "LINESTRING(-5 5,0 5,-5 5)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         1, 3, 10);

    test_one_lp<LineString, LineString, Polygon>("case33a",
        "LINESTRING(-10 10,-5 5,0 5,-5 5,-10 -10)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         1, 5, 32.882456);

    test_one_lp<LineString, LineString, Polygon>("case33b",
        "LINESTRING(0 5,-5 5,0 5)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         1, 3, 10);

    test_one_lp<LineString, LineString, Polygon>("case34",
        "LINESTRING(5 5,0 5,5 5,5 4,0 4,5 4)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         0, 0, 0);

    test_one_lp<LineString, LineString, Polygon>("case35",
        "LINESTRING(5 5,0 5,5 5,5 4,0 4,5 3)",
        "POLYGON((0 0,0 10,10 10,10 0,0 0))",
         0, 0, 0);

    test_one_lp<LineString, LineString, Polygon>("case36",
        "LINESTRING(-1 -1,10 10)",
        "POLYGON((5 5,15 15,15 5,5 5))",
        1, 2, 6 * std::sqrt(2.0));

    test_one_lp<LineString, LineString, Polygon>("case37_1",
        "LINESTRING(1 1,2 2)",
        "POLYGON((0 0,0 3,3 3,3 0,0 0),(1 1,1 2,2 2,2 1,1 1))",
        1, 2, std::sqrt(2.0));

    test_one_lp<LineString, LineString, Polygon>("case37_2",
        "LINESTRING(1 1,2 2,3 3)",
        "POLYGON((0 0,0 3,3 3,3 0,0 0),(1 1,1 2,2 2,2 1,1 1))",
        1, 2, std::sqrt(2.0));

    test_one_lp<LineString, LineString, Polygon>("case38",
        "LINESTRING(0 0,1 1,2 2,3 3)",
        "POLYGON((0 0,0 9,9 9,9 0,0 0),(0 0,2 1,2 2,1 2,0 0))",
        1, 3, 2 * std::sqrt(2.0));

    // several linestrings are in the output, the result is geometrically correct
    // still single linestring could be generated
    test_one_lp<LineString, LineString, Polygon>("case39",
        "LINESTRING(0 0,1 1,2 2,3 3)",
        "POLYGON((0 0,0 9,9 9,9 0,0 0),(0 0,2 1,2 2,1 2,0 0),(2 2,3 2,3 3,2 3,2 2))",
        2, 5, 3 * std::sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case40",
        "LINESTRING(0 0,1 1,2 2,4 4)",
        "POLYGON((0 0,0 9,9 9,9 0,0 0),(0 0,2 1,2 2,1 2,0 0),(2 2,3 2,3 3,2 3,2 2))",
        2, 5, 3 * std::sqrt(2.0));
    test_one_lp<LineString, LineString, Polygon>("case41",
        "LINESTRING(0 0,1 1,2 2,9 9)",
        "POLYGON((0 0,0 9,9 9,9 0,0 0),(0 0,2 1,2 2,1 2,0 0),(2 2,3 2,3 3,2 3,2 2),(7 7,8 7,9 9,7 8,7 7))",
        3, 7, 5 * std::sqrt(2.0));
}

template <typename P>
void test_all()
{
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::linestring<P> linestring;

    test_areal_linear<polygon, linestring>();
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double> >();

    test_ticket_10835<int>
        ("MULTILINESTRING((5239 2113,5233 2114),(4794 2205,1020 2986))",
         "MULTILINESTRING((5239 2113,5233 2114),(4794 2205,1460 2895))");

    test_ticket_10835<double>
        ("MULTILINESTRING((5239 2113,5232.52 2114.34),(4794.39 2205,1020 2986))",
         "MULTILINESTRING((5239 2113,5232.52 2114.34),(4794.39 2205,1459.78 2895))");

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<bg::model::d2::point_xy<float> >();
#endif

    return 0;
}
