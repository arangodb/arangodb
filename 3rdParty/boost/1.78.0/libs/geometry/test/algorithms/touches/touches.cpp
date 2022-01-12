// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright (c) 2012-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2013, 2014. 2015.
// Modifications copyright (c) 2013-2015, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_touches.hpp"

template <typename P>
void test_all()
{
    typedef bg::model::multi_point<P> mpoint;
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::linestring<P> linestring;
    typedef bg::model::multi_polygon<polygon> mpolygon;
    typedef bg::model::multi_linestring<linestring> mlinestring;

    // Touching at corner
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((100 100,100 200,200 200, 200 100,100 100))",
            true
        );

    // Intersecting at corner
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,101 101,100 0,0 0))",
            "POLYGON((100 100,100 200,200 200, 200 100,100 100))",
            false
        );

    // Touching at side (interior of a segment)
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((200 0,100 50,200 100,200 0))",
            true
        );

    // Touching at side (partly collinear)
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((200 20,100 20,100 80,200 80,200 20))",
            true
        );

    // Completely equal
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            false
        );

    // Spatially equal
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((0 0,0 100,100 100,100 80,100 20,100 0,0 0))",
            false
        );

    // Spatially equal (without equal segments)
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((0 0,0 50,0 100,50 100,100 100,100 50,100 0,50 0,0 0))",
            false
        );


    // Touching at side (opposite equal)
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((200 0,100 0,100 100,200 100,200 0))",
            true
        );

    // Touching at side (opposite equal - but with real "equal" turning point)
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 80,100 20,100 0,0 0))",
            "POLYGON((200 0,100 0,100 20,100 80,100 100,200 100,200 0))",
            true
        );
    // First partly collinear to side, than overlapping
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((200 20,100 20,100 50,50 50,50 80,100 80,200 80,200 20))",
            false
        );

    // Touching interior (= no touch)
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((20 20,20 80,100 50,20 20))",
            false
        );

    // Fitting in hole
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0),(40 40,60 40,60 60,40 60,40 40))",
            "POLYGON((40 40,40 60,60 60,60 40,40 40))",
            true
        );

    // mysql 21873343
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 10,10 10,10 0,0 0), (0 8, 8 5, 8 8, 0 8))",
            "POLYGON((0 8,-8 5,-8 8,0 8))",
            true
        );
    test_touches<polygon, polygon>
        (
            "POLYGON((0 0,0 10,10 10,10 0,0 0), (0 6, 6 3, 6 6, 0 6))",
            "POLYGON((0 6,-6 3,-6 6,0 6))",
            true
        );

    // Ring-Ring
    test_touches<ring, ring>
        (
            "POLYGON((0 0,0 100,100 100,100 0,0 0))",
            "POLYGON((100 100,100 200,200 200, 200 100,100 100))",
            true
        );

    // Point-Polygon
    test_touches<P, ring>("POINT(40 50)", "POLYGON((40 40,40 60,60 60,60 40,40 40))", true);
    test_touches<P, polygon>("POINT(40 50)", "POLYGON((40 40,40 60,60 60,60 40,40 40))", true);
    test_touches<P, polygon>("POINT(60 60)", "POLYGON((40 40,40 60,60 60,60 40,40 40))", true);
    test_touches<P, polygon>("POINT(50 50)", "POLYGON((40 40,40 60,60 60,60 40,40 40))", false);
    test_touches<P, polygon>("POINT(30 50)", "POLYGON((40 40,40 60,60 60,60 40,40 40))", false);

    // Point-MultiPolygon
    test_touches<P, mpolygon>("POINT(40 50)", "MULTIPOLYGON(((40 40,40 60,60 60,60 40,40 40)),((0 0,0 10,10 10,10 0)))", true);

    // Point-Linestring
    test_touches<P, linestring>("POINT(0 0)", "LINESTRING(0 0, 2 2, 10 2)", true);
    test_touches<P, linestring>("POINT(2 2)", "LINESTRING(0 0, 2 2, 10 2)", false);
    test_touches<P, linestring>("POINT(1 1)", "LINESTRING(0 0, 2 2, 10 2)", false);
    test_touches<P, linestring>("POINT(5 5)", "LINESTRING(0 0, 2 2, 10 2)", false);

    // Point-MultiLinestring
    test_touches<P, mlinestring>("POINT(0 0)", "MULTILINESTRING((0 0, 2 2, 10 2),(5 5, 6 6))", true);
    test_touches<P, mlinestring>("POINT(0 0)", "MULTILINESTRING((0 0, 2 2, 10 2),(0 0, 6 6))", false);

    // MultiPoint-Polygon
    test_touches<mpoint, ring>("MULTIPOINT(40 50, 30 50)", "POLYGON((40 40,40 60,60 60,60 40,40 40))", true);
    test_touches<mpoint, polygon>("MULTIPOINT(40 50, 50 50)", "POLYGON((40 40,40 60,60 60,60 40,40 40))", false);

    // Linestring-Linestring
    test_touches<linestring, linestring>("LINESTRING(0 0,2 0)", "LINESTRING(0 0,0 2)", true);
    test_touches<linestring, linestring>("LINESTRING(0 0,2 0)", "LINESTRING(2 0,2 2)", true);
    test_touches<linestring, linestring>("LINESTRING(0 0,2 0)", "LINESTRING(0 2,0 0)", true);
    test_touches<linestring, linestring>("LINESTRING(0 0,2 0)", "LINESTRING(2 2,2 0)", true);
    test_touches<linestring, linestring>("LINESTRING(2 0,0 0)", "LINESTRING(0 0,0 2)", true);
    test_touches<linestring, linestring>("LINESTRING(2 0,0 0)", "LINESTRING(2 0,2 2)", true);
    test_touches<linestring, linestring>("LINESTRING(2 0,0 0)", "LINESTRING(0 2,0 0)", true);
    test_touches<linestring, linestring>("LINESTRING(2 0,0 0)", "LINESTRING(2 2,2 0)", true);
    test_touches<linestring, linestring>("LINESTRING(0 0,2 0)", "LINESTRING(1 0,1 1)", true);
    test_touches<linestring, linestring>("LINESTRING(0 0,2 0)", "LINESTRING(1 1,1 0)", true);
    test_touches<linestring, linestring>("LINESTRING(2 0,0 0)", "LINESTRING(1 0,1 1)", true);
    test_touches<linestring, linestring>("LINESTRING(2 0,0 0)", "LINESTRING(1 1,1 0)", true);

    test_touches<linestring, linestring>("LINESTRING(0 0,10 0)", "LINESTRING(0 0,5 5,10 0)", true);
    test_touches<linestring, linestring>("LINESTRING(0 0,10 10)", "LINESTRING(0 0,0 5,10 5)", false);

    test_touches<linestring, linestring>("LINESTRING(0 5,5 6,10 5)", "LINESTRING(0 7,5 6,10 7)", false);
    test_touches<linestring, linestring>("LINESTRING(0 5,5 6,10 5)", "LINESTRING(10 7,5 6,0 7)", false);
    test_touches<linestring, linestring>("LINESTRING(10 5,5 6,0 5)", "LINESTRING(0 7,5 6,10 7)", false);
    test_touches<linestring, linestring>("LINESTRING(10 5,5 6,0 5)", "LINESTRING(10 7,5 6,0 7)", false);

    test_touches<linestring, linestring>("LINESTRING(0 0,1 1,2 2)", "LINESTRING(2 0,2 2,1 2,1 1)", true);
    test_touches<linestring, linestring>("LINESTRING(2 2,1 1,0 0)", "LINESTRING(2 0,2 2,1 2,1 1)", true);
    test_touches<linestring, linestring>("LINESTRING(0 0,1 1,2 2)", "LINESTRING(1 1,1 2,2 2,2 0)", true);
    test_touches<linestring, linestring>("LINESTRING(2 2,1 1,0 0)", "LINESTRING(1 1,1 2,2 2,2 0)", true);

    test_touches<linestring, linestring>("LINESTRING(0 0,1 1,0 1)", "LINESTRING(1 1,2 2,1 2,1 1)", false);

    test_touches<linestring, mlinestring>("LINESTRING(0 0,1 1,0 1)", "MULTILINESTRING((1 1,2 2),(1 2,1 1))", false);

    //Linestring-Polygon
    test_touches<linestring, polygon>("LINESTRING(10 0,15 5,10 10,5 15,5 10,0 10,5 15,5 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", true);
    test_touches<linestring, polygon>("LINESTRING(5 10,5 15,0 10,5 10,5 15,10 10,15 5,10 0)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", true);
    test_touches<linestring, polygon>("LINESTRING(5 10,5 15,0 10,5 10,5 15,10 10,5 5,10 0)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_touches<linestring, ring>("LINESTRING(0 15,5 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_touches<linestring, polygon>("LINESTRING(0 15,5 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_touches<linestring, polygon>("LINESTRING(0 15,5 10,5 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_touches<linestring, polygon>("LINESTRING(10 15,5 10,0 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);

    test_touches<linestring, polygon>("LINESTRING(0 0,3 3)", "POLYGON((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 9,1 9,0 0))", true);
    
    test_touches<linestring, mpolygon>("LINESTRING(-1 -1,3 3)", "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 9,1 9,0 0)))", true);

    test_touches<mlinestring, mpolygon>("MULTILINESTRING((0 0,11 11))", "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(0 0,9 1,9 9,1 9,0 0)))", false);
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}

/*
with viewy as
(
select geometry::STGeomFromText('POLYGON((0 0,0 100,100 100,100 0,0 0))',0) as p
     , geometry::STGeomFromText('POLYGON((200 0,100 50,200 100,200 0))',0) as q
)
-- select p from viewy union all select q from viewy
select p.STTouches(q) from viewy
*/
