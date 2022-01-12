// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2020, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_intersection_pointlike_areal
#endif

#include <iostream>
#include <algorithm>

#include <boost/test/included/unit_test.hpp>

#include "../test_set_ops_pointlike.hpp"

#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/ring.hpp>

typedef bg::model::point<double, 2, bg::cs::cartesian> Pt;
typedef bg::model::polygon<Pt> Po;
typedef bg::model::ring<Pt> R;
typedef bg::model::multi_point<Pt> MPt;
typedef bg::model::multi_polygon<Po> MPo;

BOOST_AUTO_TEST_CASE( test_intersection_point_ring )
{
    typedef test_set_op_of_pointlike_geometries
        <
            Pt, R, MPt, bg::overlay_intersection
        > tester;

    tester::apply(
        "pt-r-01",
        from_wkt<Pt>("POINT(0 0)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(0 0)")
    );

    tester::apply(
        "pt-r-02",
        from_wkt<Pt>("POINT(1 1)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(1 1)")
    );

    tester::apply(
        "pt-r-03",
        from_wkt<Pt>("POINT(6 6)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT()")
    );
}


BOOST_AUTO_TEST_CASE( test_intersection_point_polygon )
{
    typedef test_set_op_of_pointlike_geometries
        <
            Pt, Po, MPt, bg::overlay_intersection
        > tester;

    tester::apply(
        "pt-po-01",
        from_wkt<Pt>("POINT(0 0)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 3 1, 3 3, 1 3, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(0 0)")
    );

    tester::apply(
        "pt-po-02",
        from_wkt<Pt>("POINT(1 1)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 3 1, 3 3, 1 3, 0 0))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-po-03",
        from_wkt<Pt>("POINT(3 3)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 3 1, 3 3, 1 3, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(3 3)")
    );

    tester::apply(
        "pt-po-04",
        from_wkt<Pt>("POINT(4 4)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 3 1, 3 3, 1 3, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(4 4)")
    );

    tester::apply(
        "pt-po-05",
        from_wkt<Pt>("POINT(6 6)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 3 1, 3 3, 1 3, 0 0))"),
        from_wkt<MPt>("MULTIPOINT()")
    );
}


BOOST_AUTO_TEST_CASE( test_intersection_point_multipolygon )
{
    typedef test_set_op_of_pointlike_geometries
        <
            Pt, MPo, MPt, bg::overlay_intersection
        > tester;

    tester::apply(
        "pt-mpo-01",
        from_wkt<Pt>("POINT(0 0)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))"),
        from_wkt<MPt>("MULTIPOINT(0 0)")
    );

    tester::apply(
        "pt-mpo-02",
        from_wkt<Pt>("POINT(1 1)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))"),
        from_wkt<MPt>("MULTIPOINT(1 1)")
    );

    tester::apply(
        "pt-mpo-03",
        from_wkt<Pt>("POINT(2 2)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))"),
        from_wkt<MPt>("MULTIPOINT(2 2)")
    );

    tester::apply(
        "pt-mpo-04",
        from_wkt<Pt>("POINT(3 3)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))"),
        from_wkt<MPt>("MULTIPOINT()")
    );
}

BOOST_AUTO_TEST_CASE( test_intersection_multipoint_ring )
{
    typedef test_set_op_of_pointlike_geometries
        <
            MPt, R, MPt, bg::overlay_intersection
        > tester;

    tester::apply(
        "mpt-r-01",
        from_wkt<MPt>("MULTIPOINT(0 0, 1 1, 6 6)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(0 0, 1 1)")
    );
}


BOOST_AUTO_TEST_CASE( test_intersection_multipoint_polygon )
{
    typedef test_set_op_of_pointlike_geometries
        <
            MPt, Po, MPt, bg::overlay_intersection
        > tester;

    tester::apply(
        "mpt-po-01",
        from_wkt<MPt>("MULTIPOINT(0 0, 1 1, 3 3, 4 4, 6 6)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 3 1, 3 3, 1 3, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(0 0, 3 3, 4 4)")
    );
}


BOOST_AUTO_TEST_CASE( test_intersection_multipoint_multipolygon )
{
    typedef test_set_op_of_pointlike_geometries
        <
            MPt, MPo, MPt, bg::overlay_intersection
        > tester;

    tester::apply(
        "mpt-mpo-01",
        from_wkt<MPt>("MULTIPOINT(0 0, 1 1, 2 2, 3 3, 4 4, 5 5, 6 6)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),((0 0, 1 2, 2 2, 2 1, 0 0)))"),
        // NOTE: This is caused by the fact that intersection(MPt, MPt)
        //       used internally does not filter duplicates out.
        from_wkt<MPt>("MULTIPOINT(0 0, 0 0, 1 1, 2 2, 4 4, 5 5)")
    );
}