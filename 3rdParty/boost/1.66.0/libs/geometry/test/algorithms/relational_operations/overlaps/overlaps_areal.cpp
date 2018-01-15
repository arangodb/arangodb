// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014, 2015.
// Modifications copyright (c) 2014-2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_overlaps.hpp"

template <typename P>
void test_aa()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<poly, poly>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((3 3,3 9,9 9,9 3,3 3))", true);
    test_geometry<poly, poly>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((5 5,5 9,9 9,9 5,5 5))", false);
    test_geometry<poly, poly>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((3 3,3 5,5 5,5 3,3 3))", false);

    test_geometry<poly, mpoly>("POLYGON((0 0,0 5,5 5,5 0,0 0))",
                               "MULTIPOLYGON(((3 3,3 5,5 5,5 3,3 3)),((5 5,5 6,6 6,6 5,5 5)))",
                               true);
    test_geometry<mpoly, mpoly>("MULTIPOLYGON(((3 3,3 5,5 5,5 3,3 3)),((0 0,0 3,3 3,3 0,0,0)))",
                                "MULTIPOLYGON(((3 3,3 5,5 5,5 3,3 3)),((5 5,5 6,6 6,6 5,5 5)))",
                                true);

    // related to https://svn.boost.org/trac/boost/ticket/10912
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2))",
                              "POLYGON((3 3,3 9,9 9,9 3,3 3))",
                              true);
    test_geometry<poly, poly>("POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2),(6 6,8 6,8 8,6 8,6 6))",
                              "POLYGON((0 0,0 5,5 5,5 0,0 0))",
                              true);

    test_geometry<mpoly, poly>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                               "POLYGON((0 0,0 5,5 5,5 0,0 0))",
                               false);
    test_geometry<mpoly, poly>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                               "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                               false);

    // mysql 21872795
    test_geometry<poly, poly>("POLYGON((2 2,2 8,8 8,8 2,2 2))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(8 8,4 6,4 4,8 8))",
                              true);
    test_geometry<poly, poly>("POLYGON((2 2,2 8,8 8,8 2,2 2))",
                              "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 4,4 6,2 2))",
                              true);
}

template <typename P>
void test_2d()
{
    test_aa<P>();
}

int test_main( int , char* [] )
{
    test_2d<bg::model::d2::point_xy<int> >();
    test_2d<bg::model::d2::point_xy<double> >();

#if defined(HAVE_TTMATH)
    test_2d<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
