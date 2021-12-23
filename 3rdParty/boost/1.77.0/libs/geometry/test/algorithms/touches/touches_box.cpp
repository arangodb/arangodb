// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright (c) 2012-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2013, 2014, 2015.
// Modifications copyright (c) 2013-2015, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_touches.hpp"

template <typename P>
void test_all()
{
    typedef bg::model::box<P> box;

    test_touches<box, box>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((5 1,5 2,6 2,6 1,5 1))", true);
    test_touches<box, box>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((4 1,4 2,5 2,5 1,4 1))", false);
    test_touches<box, box>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((4 1,4 2,6 2,6 1,4 1))", false);

    // Point-size
    test_touches<box, box>("POLYGON((0 0,0 5,5 5,5 0,0 0))", "POLYGON((5 5,5 5,5 5,5 5,5 5))", true);
    // TODO: should it be TRUE?
    test_touches<box, box>("POLYGON((5 5,5 5,5 5,5 5,5 5))", "POLYGON((5 5,5 5,5 5,5 5,5 5))", true);
}

template <typename P>
void test_box_3d()
{
    typedef bg::model::box<P> box;

    check_touches<box, box>(box(P(0,0,0),P(5,5,5)), box(P(5,1,2),P(6,6,6)),
                            "box(P(0,0,0),P(5,5,5))", "box(P(5,1,2),P(6,6,6))",
                            true);

    check_touches<box, box>(box(P(0,0,0),P(5,5,5)), box(P(5,5,5),P(6,6,6)),
                            "box(P(0,0,0),P(5,5,5))", "box(P(5,5,5),P(6,6,6))",
                            true);
}


int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<double> >();
    test_box_3d<bg::model::point<double, 3, bg::cs::cartesian> >();

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
