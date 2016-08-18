// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014, 2015.
// Modifications copyright (c) 2014-2015 Oracle and/or its affiliates.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

#include "test_within.hpp"


#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

template <typename P>
void test_l_a()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;
    typedef bg::model::multi_polygon<poly> mpoly;

    // B,I
    test_geometry<ls, ring>("LINESTRING(0 0, 2 2)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);

    // B,I
    test_geometry<ls, poly>("LINESTRING(0 0, 2 2)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    // I
    test_geometry<ls, poly>("LINESTRING(1 1, 2 2)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    // I,E
    test_geometry<ls, poly>("LINESTRING(1 1, 6 6)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);
    // B
    test_geometry<ls, poly>("LINESTRING(0 0, 5 0)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);
    test_geometry<ls, poly>("LINESTRING(0 0, 0 5)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);
    // E
    test_geometry<ls, poly>("LINESTRING(6 0, 6 5)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);

    // BIBIB
    test_geometry<ls, mpoly>("LINESTRING(0 0, 10 10)", "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))", true);
    // BIBEBIB
    test_geometry<ls, mpoly>("LINESTRING(0 0, 10 10)", "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((6 6,5 10,10 10,10 5,6 6)))", false);

    // MySQL report 18.12.2014 (https://svn.boost.org/trac/boost/ticket/10887)
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 2)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                             true);
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 5)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                             true);
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 0)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                             true);
    // MySQL report 18.12.2014 - extended
    test_geometry<ls, mpoly>("LINESTRING(5 -2,5 0)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)),((5 0,7 1,7 -1,5 0)))",
                             true);
    test_geometry<ls, mpoly>("LINESTRING(0 0,5 0)",
                             "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)),((5 0,7 1,7 -1,5 0)))",
                             false);
    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,4 -2,5 0),(5 -2,6 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              true);
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,0 1,5 0),(0 0,0 -1,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              false);
    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,4 -2,5 0),(6 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              true);
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,0 1,5 0),(0 -1,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              false);
    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,5 0),(5 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              false);
    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,5 0),(0 0,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              false);

    // BI
    test_geometry<mls, poly>("MULTILINESTRING((0 0,2 2),(2 2,3 3))", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    // I E
    test_geometry<mls, poly>("MULTILINESTRING((1 1,2 2),(6 6,7 7))", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);

    // I I
    test_geometry<mls, mpoly>("MULTILINESTRING((1 1,5 5),(6 6,7 7))",
                              "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))",
                              true);
    // I E
    test_geometry<mls, mpoly>("MULTILINESTRING((1 1,5 5),(11 11,12 12))",
                              "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))",
                              false);
}

template <typename P>
void test_all()
{
    test_l_a<P>();
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();


#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
