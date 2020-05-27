// Boost.Geometry

// Copyright (c) 2016 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_crosses.hpp"

#include <algorithms/overlay/overlay_cases.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/geometries/geometries.hpp>


template <typename P>
void test_linestring_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::polygon<P> ring;

    test_geometry<ls, poly>("LINESTRING(11 0,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_geometry<ls, ring>("LINESTRING(11 0,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_geometry<ls, poly>("LINESTRING(0 0,10 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_geometry<ls, poly>("LINESTRING(5 0,5 5,10 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_geometry<ls, poly>("LINESTRING(5 1,5 5,9 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_geometry<ls, poly>("LINESTRING(11 1,11 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);

    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            false);

    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9,1 9,1 1,9 1)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            false);

    test_geometry<ls, poly>("LINESTRING(0 0,10 0,10 10,0 10,0 0)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                            false);
}

template <typename P>
void test_linestring_multi_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,10 9)",
                             "MULTIPOLYGON(((0 20,0 30,10 30,10 20,0 20)),((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)))",
                             false);
}

template <typename P>
void test_multi_linestring_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<mls, poly>("MULTILINESTRING((11 11, 20 20),(5 7, 4 1))",
                             "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2))",
                             true);

    test_geometry<mls, ring>("MULTILINESTRING((6 6,15 15),(0 0, 7 7))",
                             "POLYGON((5 5,5 15,15 15,15 5,5 5))",
                             true);

    test_geometry<mls, poly>("MULTILINESTRING((3 10.031432746397092, 1 5, 1 10.013467818052765, 3 4, 7 8, 6 10.035925377760330, 10 2))",
                             "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                             false);
}

template <typename P>
void test_multi_linestring_multi_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10,0 10,0 0),(2 2,5 5,2 8,2 2))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(2 2,5 5,2 8,2 2)))",
                              false);

    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10),(10 10,0 10,0 0),(20 20,50 50,20 80,20 20))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                              false);

    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,4 -2,5 0),(5 -2,6 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              false);
}


template <typename P>
void test_all()
{
    test_linestring_polygon<P>();
    test_linestring_multi_polygon<P>();
    test_multi_linestring_polygon<P>();
    test_multi_linestring_multi_polygon<P>();
}


int test_main( int , char* [] )
{
    test_all<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();

#if defined(HAVE_TTMATH)
    test_cs<bg::model::point<ttmath_big, 2, bg::cs::spherical_equatorial<bg::degree> > >();
#endif

    return 0;
}
