// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014 Oracle and/or its affiliates.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

#include "test_crosses.hpp"

template <typename P>
void test_pl()
{
    /*typedef bg::model::multi_point<P> mpt;
    typedef bg::model::linestring<P> ls;

    // not implemented yet
    test_geometry<mpt, ls>("MULTIPOINT(0 0,1 1)", "LINESTRING(0 0,1 0,3 3)", true);
    test_geometry<mpt, ls>("MULTIPOINT(0 0,1 1)", "LINESTRING(0 0,1 1,3 3)", false);*/
}

template <typename P>
void test_pa()
{
    /*typedef bg::model::multi_point<P> mpt;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    // not implemented yet
    test_geometry<mpt, poly>("MULTIPOINT(0 0,6 6)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    test_geometry<mpt, poly>("MULTIPOINT(0 0,1 1)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);*/
}

template <typename P>
void test_ll()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<ls, ls>("LINESTRING(0 0,2 2,4 4)", "LINESTRING(0 1,2 1,3 1)", true);
    test_geometry<ls, ls>("LINESTRING(0 0,2 2)", "LINESTRING(0 1,2 1)", true);
    test_geometry<ls, ls>("LINESTRING(0 0,2 2,4 4)", "LINESTRING(0 1,1 1,2 2,3 2)", false);

    test_geometry<ls, mls>("LINESTRING(0 0,2 2,4 4)", "MULTILINESTRING((0 1,4 1),(0 2,4 2))", true);
    test_geometry<mls, ls>("MULTILINESTRING((0 1,4 1),(0 2,4 2))", "LINESTRING(0 0,2 2,4 4)", true);

    test_geometry<mls, mls>("MULTILINESTRING((0 0,2 2,4 4),(3 0,3 4))", "MULTILINESTRING((0 1,4 1),(0 2,4 2))", true);

    // spike - boundary and interior on the same point
    test_geometry<ls, ls>("LINESTRING(3 7, 8 8, 2 6)", "LINESTRING(5 7, 10 7, 0 7)", true);
}

template <typename P>
void test_la()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<ls, ring>("LINESTRING(0 0, 10 10)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    test_geometry<ls, poly>("LINESTRING(0 0, 10 10)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    test_geometry<ls, mpoly>("LINESTRING(0 0, 10 10)", "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)))", true);

    test_geometry<ls, poly>("LINESTRING(0 0, 10 0)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);
    test_geometry<ls, poly>("LINESTRING(1 1, 5 5)", "POLYGON((0 0,0 5,5 5,5 0,0 0))", false);

    test_geometry<mls, ring>("MULTILINESTRING((1 1, 5 5),(6 6,7 7))", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);
    test_geometry<mls, poly>("MULTILINESTRING((1 1, 5 5),(6 6,7 7))", "POLYGON((0 0,0 5,5 5,5 0,0 0))", true);    
    test_geometry<mls, mpoly>("MULTILINESTRING((1 1, 5 5),(6 6,7 7))", "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0)))", true);
}

template <typename P>
void test_2d()
{
    test_pl<P>();
    test_pa<P>();
    test_ll<P>();
    test_la<P>();
}

int test_main( int , char* [] )
{
    test_2d<bg::model::d2::point_xy<int> >();
    test_2d<bg::model::d2::point_xy<double> >();

#if defined(HAVE_TTMATH)
    test_2d<bg::model::d2::point_xy<ttmath_big> >();
#endif

   //test_3d<bg::model::point<double, 3, bg::cs::cartesian> >();

    return 0;
}
