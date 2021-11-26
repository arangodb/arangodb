// Boost.Geometry
// Unit Test

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_difference.hpp"

void test_all()
{
    typedef bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > point;
    typedef bg::model::linestring<point> linestring;
    typedef bg::model::polygon<point> polygon;

    // https://github.com/boostorg/geometry/issues/619
    
    test_one_lp<linestring, linestring, polygon>(
        "issue_619",
        "LINESTRING(-106.373725 39.638846, -106.373486 39.639362, -106.368378 39.614603)",
        "POLYGON((-106.374074 39.638593, -106.373626 39.639230, -106.373594 39.639232, "
                 "-106.373366 39.638502, -106.373299 39.638459, -106.373369 39.638382, "
                 "-106.374074 39.638593))",
        2, 5, 0.00044107988528133710);
}

int test_main(int, char* [])
{
    test_all();

    return 0;
}
