// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2014, 2015, 2016.
// Modifications copyright (c) 2014-2016 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_within.hpp"


#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

template <typename P1, typename P2>
void test_a_a()
{
    typedef bg::model::polygon<P1> poly1;
    typedef bg::model::ring<P1> ring1;
    typedef bg::model::multi_polygon<poly1> mpoly1;
    typedef bg::model::polygon<P2> poly2;
    typedef bg::model::ring<P2> ring2;
    typedef bg::model::multi_polygon<poly2> mpoly2;

    test_geometry<ring1, ring2>("POLYGON((0 0,0 2,2 2,2 0,0 0))", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    test_geometry<ring1, poly2>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    test_geometry<poly1, ring2>("POLYGON((0 0,0 6,6 6,6 0,0 0))", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);

    test_geometry<poly1, poly2>("POLYGON((0 0,0 9,9 9,9 0,0 0),(3 3,6 3,6 6,3 6,3 3))",
                                "POLYGON((0 0,0 9,9 9,9 0,0 0),(3 3,6 3,6 6,3 6,3 3))", true);
    test_geometry<poly1, poly2>("POLYGON((0 0,0 9,9 9,9 0,0 0),(3 3,6 3,6 6,3 6,3 3))",
                                "POLYGON((0 0,0 9,9 9,9 0,0 0),(4 4,5 4,5 5,4 5,4 4))", true);
    test_geometry<poly1, poly2>("POLYGON((1 1,1 8,8 8,8 1,1 1),(3 3,6 3,6 6,3 6,3 3))",
                                "POLYGON((0 0,0 9,9 9,9 0,0 0),(3 3,6 3,6 6,3 6,3 3))", true);
    test_geometry<poly1, poly2>("POLYGON((1 1,1 8,8 8,8 1,1 1),(3 3,6 3,6 6,3 6,3 3))",
                                "POLYGON((0 0,0 9,9 9,9 0,0 0),(4 4,5 4,5 5,4 5,4 4))", true);
    
    test_geometry<ring1, mpoly2>("POLYGON((0 0,0 2,2 2,2 0,0 0))",
                                 "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))", true);
    test_geometry<poly1, mpoly2>("POLYGON((0 0,0 2,2 2,2 0,0 0))",
                                 "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))", true);

    test_geometry<mpoly1, ring2>("MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))",
                                 "POLYGON((0 0,0 10,10 10,10 0,0 0))", true);
    test_geometry<mpoly1, poly2>("MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((15 15,15 110,110 110,110 15,15 15)))",
                                 "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);

    test_geometry<mpoly1, poly2>("MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((3 3,3 4,4 3,3 3)))",
                                 "POLYGON((0 0,0 10,10 10,10 0,0 0),(3 3,4 3,4 4,3 4,3 3))", false);

    test_geometry<mpoly1, mpoly2>("MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((3 3,3 4,4 3,3 3)))",
                                  "MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((3 3,3 4,4 3,3 3)))", true);
    test_geometry<mpoly1, mpoly2>("MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((3 3,3 4,4 3,3 3)))",
                                  "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))", true);
    test_geometry<mpoly1, mpoly2>("MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)),((5 5,5 10,10 10,10 5,5 5)))",
                                  "MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((3 3,3 4,4 3,3 3)))", false);

    // https://svn.boost.org/trac/boost/ticket/10912
    test_geometry<poly1, poly2>("POLYGON((0 0,0 5,5 5,5 0,0 0))",
                                "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2),(6 6,8 6,8 8,6 8,6 6))",
                                false);
    test_geometry<poly1, poly2>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                                "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,2 4,4 4,4 2,2 2))",
                                false);

    test_geometry<poly1, mpoly2>("POLYGON((0 0,0 5,5 5,5 0,0 0))",
                                 "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                                 true);
    test_geometry<poly1, mpoly2>("POLYGON((0 0,0 10,10 10,10 0,0 0))",
                                 "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((0 0,0 -10,-10 -10,-10 0,0 0)))",
                                 true);
}

template <typename P1, typename P2>
void test_all()
{
    test_a_a<P1, P2>();
}

template <typename P>
void test_all()
{
    test_a_a<P, P>();
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();
    test_all<bg::model::d2::point_xy<double>, bg::model::point<double, 2, bg::cs::cartesian> >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
