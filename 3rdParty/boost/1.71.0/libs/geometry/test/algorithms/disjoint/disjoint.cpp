// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2015.
// Modifications copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_disjoint.hpp"

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include <test_common/test_point.hpp>

#include <algorithms/overlay/overlay_cases.hpp>

#include <algorithms/predef_relop.hpp>


template <typename P>
void test_all()
{
    // Test triangles for polygons/rings, boxes
    // Note that intersections are tested elsewhere, they don't need
    // thorough test at this place
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::ring<P> ring;

    // Four times same test with other types
    test_disjoint<polygon, polygon>("disjoint_simplex_pp", disjoint_simplex[0], disjoint_simplex[1], true);
    test_disjoint<ring, polygon>("disjoint_simplex_rp", disjoint_simplex[0], disjoint_simplex[1], true);
    test_disjoint<ring, ring>("disjoint_simplex_rr", disjoint_simplex[0], disjoint_simplex[1], true);
    test_disjoint<polygon, ring>("disjoint_simplex_pr", disjoint_simplex[0], disjoint_simplex[1], true);

    test_disjoint<polygon, polygon>("ticket_8310a", ticket_8310a[0], ticket_8310a[1], false);
    test_disjoint<polygon, polygon>("ticket_8310b", ticket_8310b[0], ticket_8310b[1], false);
    test_disjoint<polygon, polygon>("ticket_8310c", ticket_8310c[0], ticket_8310c[1], false);

    // Testing touch
    test_disjoint<polygon, polygon>("touch_simplex_pp", touch_simplex[0], touch_simplex[1], false);

    // Test if within(a,b) returns false for disjoint
    test_disjoint<ring, ring>("within_simplex_rr1", within_simplex[0], within_simplex[1], false);
    test_disjoint<ring, ring>("within_simplex_rr2", within_simplex[1], within_simplex[0], false);

    // Linear
    typedef bg::model::linestring<P> ls;
    typedef bg::model::segment<P> segment;
    test_disjoint<ls, ls>("ls/ls 1", "linestring(0 0,1 1)", "linestring(1 0,0 1)", false);
    test_disjoint<ls, ls>("ls/ls 2", "linestring(0 0,1 1)", "linestring(1 0,2 1)", true);
    test_disjoint<segment, segment>("s/s 1", "linestring(0 0,1 1)", "linestring(1 0,0 1)", false);
    test_disjoint<segment, segment>("s/s 2", "linestring(0 0,1 1)", "linestring(1 0,2 1)", true);

    // Test degenerate segments (patched by Karsten Ahnert on 2012-07-25)
    test_disjoint<segment, segment>("s/s 3", "linestring(0 0,0 0)", "linestring(1 0,0 1)", true);
    test_disjoint<segment, segment>("s/s 4", "linestring(0 0,0 0)", "linestring(0 0,0 0)", false);
    test_disjoint<segment, segment>("s/s 5", "linestring(0 0,0 0)", "linestring(1 0,1 0)", true);
    test_disjoint<segment, segment>("s/s 6", "linestring(0 0,0 0)", "linestring(0 1,0 1)", true);

    // Collinear opposite
    test_disjoint<ls, ls>("ls/ls co", "linestring(0 0,2 2)", "linestring(1 1,0 0)", false);
    // Collinear opposite and equal
    test_disjoint<ls, ls>("ls/ls co-e", "linestring(0 0,1 1)", "linestring(1 1,0 0)", false);


    // Degenerate linestrings
    {
        // Submitted by Zachary on the Boost.Geometry Mailing List, on 2012-01-29
        std::string const a = "linestring(100 10, 0 10)";
        std::string const b = "linestring(50 10, 50 10)"; // one point only, with same y-coordinate
        std::string const c = "linestring(100 10, 100 10)"; // idem, at left side
        test_disjoint<ls, ls>("dls/dls 1", a, b, false);
        test_disjoint<ls, ls>("dls/dls 2", b, a, false);
        test_disjoint<segment, segment>("ds/ds 1", a, b, false);
        test_disjoint<segment, segment>("ds/ds 2", b, a, false);
        test_disjoint<ls, ls>("dls/dls 1", a, c, false);
    }

    // Linestrings making angles normally ignored
    {
        // These (non-disjoint) cases
        // correspond to the test "segment_intersection_collinear"

        // Collinear ('a')
        //       a1---------->a2
        // b1--->b2
        test_disjoint<ls, ls>("n1", "linestring(2 0,0 6)", "linestring(0 0,2 0)", false);

        //       a1---------->a2
        //                    b1--->b2
        test_disjoint<ls, ls>("n7", "linestring(2 0,6 0)", "linestring(6 0,8 0)", false);

        // Collinear - opposite ('f')
        //       a1---------->a2
        // b2<---b1
        test_disjoint<ls, ls>("o1", "linestring(2 0,6 0)", "linestring(2 0,0 0)", false);
    }

    {
        // Starting in the middle ('s')
        //           b2
        //           ^
        //           |
        //           |
        // a1--------b1----->a2
        test_disjoint<ls, ls>("case_s", "linestring(0 0,4 0)", "linestring(2 0,2 2)", false);

        // Collinear, but disjoint
        test_disjoint<ls, ls>("c-d", "linestring(2 0,6 0)", "linestring(7 0,8 0)", true);

        // Parallel, disjoint
        test_disjoint<ls, ls>("c-d", "linestring(2 0,6 0)", "linestring(2 1,6 1)", true);

        // Error still there until 1.48 (reported "error", was reported to disjoint, so that's why it did no harm)
        test_disjoint<ls, ls>("case_recursive_boxes_1",
            "linestring(10 7,10 6)", "linestring(10 10,10 9)", true);

    }

    // TODO test_disjoint<segment, ls>("s/ls 1", "linestring(0 0,1 1)", "linestring(1 0,0 1)", false);
    // TODO test_disjoint<segment, ls>("s/ls 2", "linestring(0 0,1 1)", "linestring(1 0,2 1)", true);
    // TODO test_disjoint<ls, segment>("ls/s 1", "linestring(0 0,1 1)", "linestring(1 0,0 1)", false);
    // TODO test_disjoint<ls, segment>("ls/s 2", "linestring(0 0,1 1)", "linestring(1 0,2 1)", true);

    // 22.01.2015
    test_disjoint<ls, ls>("col-op", "LINESTRING(5 5,10 10)", "LINESTRING(6 6,3 3)", false);
    test_disjoint<ls, ls>("col-op", "LINESTRING(5 5,2 8)", "LINESTRING(4 6,7 3)", false);

    test_disjoint<ls, polygon>("col-op", "LINESTRING(10 10,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_disjoint<ls, polygon>("col-op", "LINESTRING(9 10,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);

    // assertion failure in 1.57
    test_disjoint<ls, ls>("point_ll_assert_1_57",
                          "LINESTRING(-2305843009213693956 4611686018427387906, -33 -92, 78 83)",
                          "LINESTRING(31 -97, -46 57, -20 -4)",
                          false);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();

#ifdef HAVE_TTMATH
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif


    return 0;
}
