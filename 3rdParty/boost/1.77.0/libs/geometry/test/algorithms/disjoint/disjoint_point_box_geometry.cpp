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
    typedef bg::model::box<P> box;

    test_disjoint<P, P>("pp1", "point(1 1)", "point(1 1)", false);
    test_disjoint<P, P>("pp2", "point(1 1)", "point(1.001 1)", true);

    // left-right
    test_disjoint<box, box>("bb1", "box(1 1, 2 2)", "box(3 1, 4 2)", true);
    test_disjoint<box, box>("bb2", "box(1 1, 2 2)", "box(2 1, 3 2)", false);
    test_disjoint<box, box>("bb3", "box(1 1, 2 2)", "box(2 2, 3 3)", false);
    test_disjoint<box, box>("bb4", "box(1 1, 2 2)", "box(2.001 2, 3 3)", true);

    // up-down
    test_disjoint<box, box>("bb5", "box(1 1, 2 2)", "box(1 3, 2 4)", true);
    test_disjoint<box, box>("bb6", "box(1 1, 2 2)", "box(1 2, 2 3)", false);
    // right-left
    test_disjoint<box, box>("bb7", "box(1 1, 2 2)", "box(0 1, 1 2)", false);
    test_disjoint<box, box>("bb8", "box(1 1, 2 2)", "box(0 1, 1 2)", false);

    // point-box
    test_disjoint<P, box>("pb1", "point(1 1)", "box(0 0, 2 2)", false);
    test_disjoint<P, box>("pb2", "point(2 2)", "box(0 0, 2 2)", false);
    test_disjoint<P, box>("pb3", "point(2.0001 2)", "box(1 1, 2 2)", true);
    test_disjoint<P, box>("pb4", "point(0.9999 2)", "box(1 1, 2 2)", true);

    // box-point (to test reverse compiling)
    test_disjoint<box, P>("bp1", "box(1 1, 2 2)", "point(2 2)", false);

    // Test triangles for polygons/rings, boxes
    // Note that intersections are tested elsewhere, they don't need
    // thorough test at this place
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::ring<P> ring;

    // Testing overlap (and test compiling with box)
    test_disjoint<polygon, polygon>("overlaps_box_pp", overlaps_box[0], overlaps_box[1], false);
    test_disjoint<box, polygon>("overlaps_box_bp", overlaps_box[0], overlaps_box[1], false);
    test_disjoint<box, ring>("overlaps_box_br", overlaps_box[0], overlaps_box[1], false);
    test_disjoint<polygon, box>("overlaps_box_pb", overlaps_box[1], overlaps_box[0], false);
    test_disjoint<ring, box>("overlaps_box_rb", overlaps_box[1], overlaps_box[0], false);

    test_disjoint<P, ring>("point_ring1", "POINT(0 0)", "POLYGON((0 0,3 3,6 0,0 0))", false);
    test_disjoint<P, ring>("point_ring2", "POINT(3 1)", "POLYGON((0 0,3 3,6 0,0 0))", false);
    test_disjoint<P, ring>("point_ring3", "POINT(0 3)", "POLYGON((0 0,3 3,6 0,0 0))", true);
    test_disjoint<P, polygon>("point_polygon1", "POINT(0 0)", "POLYGON((0 0,3 3,6 0,0 0))", false);
    test_disjoint<P, polygon>("point_polygon2", "POINT(3 1)", "POLYGON((0 0,3 3,6 0,0 0))", false);
    test_disjoint<P, polygon>("point_polygon3", "POINT(0 3)", "POLYGON((0 0,3 3,6 0,0 0))", true);

    test_disjoint<ring, P>("point_ring2", "POLYGON((0 0,3 3,6 0,0 0))", "POINT(0 0)", false);
    test_disjoint<polygon, P>("point_polygon2", "POLYGON((0 0,3 3,6 0,0 0))", "POINT(0 0)", false);


    // Problem described by Volker/Albert 2012-06-01
    test_disjoint<polygon, box>("volker_albert_1",
        "POLYGON((1992 3240,1992 1440,3792 1800,3792 3240,1992 3240))",
        "BOX(1941 2066, 2055 2166)", false);

    test_disjoint<polygon, box>("volker_albert_2",
        "POLYGON((1941 2066,2055 2066,2055 2166,1941 2166))",
        "BOX(1941 2066, 2055 2166)", false);
}


template <typename P>
void test_3d()
{
    typedef bg::model::box<P> box;

    test_disjoint<P, P>("pp 3d 1", "point(1 1 1)", "point(1 1 1)", false);
    test_disjoint<P, P>("pp 3d 2", "point(1 1 1)", "point(1.001 1 1)", true);

    test_disjoint<box, box>("bb1", "box(1 1 1, 2 2 2)", "box(3 1 1, 4 2 1)", true);
    test_disjoint<box, box>("bb2", "box(1 1 1, 2 2 2)", "box(2 1 1, 3 2 1)", false);
    test_disjoint<box, box>("bb3", "box(1 1 1, 2 2 2)", "box(2 2 1, 3 3 1)", false);
    test_disjoint<box, box>("bb4", "box(1 1 1, 2 2 2)", "box(2.001 2 1, 3 3 1)", true);

}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<float> >();
    
    test_all<bg::model::d2::point_xy<double> >();

    test_3d<bg::model::point<double, 3, bg::cs::cartesian> >();
    
    return 0;
}
