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
    typedef bg::model::polygon<P> polygon;

    // Just a normal polygon
    test_self_touches<polygon>("POLYGON((0 0,0 4,1.5 2.5,2.5 1.5,4 0,0 0))", false);

    // Self intersecting
    test_self_touches<polygon>("POLYGON((1 2,1 1,2 1,2 2.25,3 2.25,3 0,0 0,0 3,3 3,2.75 2,1 2))", false);

    // Self touching at a point
    test_self_touches<polygon>("POLYGON((0 0,0 3,2 3,2 2,1 2,1 1,2 1,2 2,3 2,3 0,0 0))", true);

    // Self touching at a segment
    test_self_touches<polygon>("POLYGON((0 0,0 3,2 3,2 2,1 2,1 1,2 1,2 2.5,3 2.5,3 0,0 0))", true);
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
