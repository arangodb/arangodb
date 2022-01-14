// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014, 2015, 2017.
// Modifications copyright (c) 2014-2017 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_overlaps.hpp"

template <typename P>
void test_pp()
{
    typedef bg::model::multi_point<P> mpt;

    test_geometry<P, P>("POINT(0 0)", "POINT(0 0)", false);
    test_geometry<P, P>("POINT(0 0)", "POINT(1 1)", false);

    test_geometry<P, mpt>("POINT(0 0)", "MULTIPOINT(0 0, 1 1)", false);

    test_geometry<mpt, P>("MULTIPOINT(0 0, 1 1)", "POINT(0 0)", false);

    test_geometry<mpt, mpt>("MULTIPOINT(0 0,1 1,2 2)", "MULTIPOINT(1 1,3 3,4 4)", true);
    test_geometry<mpt, mpt>("MULTIPOINT(0 0,1 1,2 2)", "MULTIPOINT(1 1,2 2)", false);
}

template <typename P>
void test_ll()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<ls, ls>("LINESTRING(0 0,2 2,3 1)", "LINESTRING(1 1,2 2,4 4)", true);
    test_geometry<ls, ls>("LINESTRING(0 0,2 2,4 0)", "LINESTRING(0 1,2 1,3 2)", false);

    test_geometry<ls, mls>("LINESTRING(0 0,2 2,3 1)", "MULTILINESTRING((1 1,2 2),(2 2,4 4))", true);
    test_geometry<ls, mls>("LINESTRING(0 0,2 2,3 1)", "MULTILINESTRING((1 1,2 2),(3 3,4 4))", true);
    test_geometry<ls, mls>("LINESTRING(0 0,3 3,3 1)", "MULTILINESTRING((3 3,2 2),(0 0,1 1))", false);
}

template <typename P>
void test_2d()
{
    test_pp<P>();
    test_ll<P>();
}

int test_main( int , char* [] )
{
    test_2d<bg::model::d2::point_xy<int> >();
    test_2d<bg::model::d2::point_xy<double> >();

    return 0;
}
