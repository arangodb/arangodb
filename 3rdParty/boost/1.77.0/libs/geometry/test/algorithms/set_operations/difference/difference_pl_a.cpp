// Boost.Geometry

// Copyright (c) 2020, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_difference_pointlike_areal
#endif

#include <iostream>
#include <algorithm>

#include <boost/test/included/unit_test.hpp>

#include "../test_set_ops_pointlike.hpp"

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/ring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

typedef bg::model::point<double, 2, bg::cs::cartesian> Pt;
typedef bg::model::ring<Pt> R;
typedef bg::model::polygon<Pt> Po;
typedef bg::model::multi_point<Pt> MPt;
typedef bg::model::multi_polygon<Po> MPo;

BOOST_AUTO_TEST_CASE( test_difference_point_ring )
{
    typedef test_set_op_of_pointlike_geometries
        <
            Pt, R, MPt, bg::overlay_difference
        > tester;

    tester::apply(
        "pt-r-01",
        from_wkt<Pt>("POINT(0 0)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-r-02",
        from_wkt<Pt>("POINT(3 5)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-r-03",
        from_wkt<Pt>("POINT(6 3)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(6 3)")
    );
}

BOOST_AUTO_TEST_CASE( test_difference_point_polygon )
{
    typedef test_set_op_of_pointlike_geometries
        <
            Pt, Po, MPt, bg::overlay_difference
        > tester;

    tester::apply(
        "pt-po-01",
        from_wkt<Pt>("POINT(0 0)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-po-02",
        from_wkt<Pt>("POINT(4 4)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-po-03",
        from_wkt<Pt>("POINT(3 3)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(3 3)")
    );

    tester::apply(
        "pt-po-04",
        from_wkt<Pt>("POINT(6 0)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(6 0)")
    );
}

BOOST_AUTO_TEST_CASE( test_difference_point_multipolygon )
{
    typedef test_set_op_of_pointlike_geometries
        <
            Pt, MPo, MPt, bg::overlay_difference
        > tester;

    tester::apply(
        "pt-mpo-01",
        from_wkt<Pt>("POINT(0 0)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                                   "((5 5, 5 7, 7 7, 7 5, 5 5)))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-mpo-02",
        from_wkt<Pt>("POINT(3 3)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                                   "((5 5, 5 7, 7 7, 7 5, 5 5)))"),
        from_wkt<MPt>("MULTIPOINT(3 3)")
    );

    tester::apply(
        "pt-mpo-03",
        from_wkt<Pt>("POINT(4 4)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                                   "((5 5, 5 7, 7 7, 7 5, 5 5)))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-mpo-04",
        from_wkt<Pt>("POINT(5 5)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                                   "((5 5, 5 7, 7 7, 7 5, 5 5)))"),
        from_wkt<MPt>("MULTIPOINT()")
    );

    tester::apply(
        "pt-mpo-05",
        from_wkt<Pt>("POINT(6 6)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                                   "((5 5, 5 7, 7 7, 7 5, 5 5)))"),
        from_wkt<MPt>("MULTIPOINT()")
    );
}

BOOST_AUTO_TEST_CASE( test_difference_multipoint_ring )
{
    typedef test_set_op_of_pointlike_geometries
        <
            MPt, R, MPt, bg::overlay_difference
        > tester;

    tester::apply(
        "mpt-r-01",
        from_wkt<MPt>("MULTIPOINT(0 0, 1 1, 5 5, 6 6)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(6 6)")
    );

    tester::apply(
        "mpt-r-02",
        from_wkt<MPt>("MULTIPOINT(3 5, 5 3, 3 0, 0 3, 3 6, 6 3, 3 -1, -1 3)"),
        from_wkt<R>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(3 6, 6 3, 3 -1, -1 3)")
    );
}

BOOST_AUTO_TEST_CASE( test_difference_multipoint_polygon )
{
    typedef test_set_op_of_pointlike_geometries
        <
            MPt, Po, MPt, bg::overlay_difference
        > tester;

    tester::apply(
        "mpt-po-01",
        from_wkt<MPt>("MULTIPOINT(0 0, 1 1, 4 4, 5 5, 6 6)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(1 1, 6 6)")
    );

    tester::apply(
        "mpt-po-02",
        from_wkt<MPt>("MULTIPOINT(3 5, 5 3, 3 0, 0 3, 3 6, 6 3, 3 -1, -1 3, 2 0.5, 0.5 2, 2 4, 4 2)"),
        from_wkt<Po>("POLYGON((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0))"),
        from_wkt<MPt>("MULTIPOINT(3 6, 6 3, 3 -1, -1 3)")
    );
}

BOOST_AUTO_TEST_CASE(test_difference_multipoint_multipolygon)
{
    typedef test_set_op_of_pointlike_geometries
        <
        MPt, MPo, MPt, bg::overlay_difference
        > tester;

    tester::apply(
        "mpt-mpo-01",
        from_wkt<MPt>("MULTIPOINT(0 0, 1 1, 4 4, 5 5, 6 6)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                                   "((5 5, 5 7, 7 7, 7 5, 5 5)))"),
        from_wkt<MPt>("MULTIPOINT(1 1)")
    );

    tester::apply(
        "mpt-mpo-02",
        from_wkt<MPt>("MULTIPOINT(3 5, 5 3, 3 0, 0 3, 3 6, 6 3, 3 -1, -1 3, 2 0.5, 0.5 2, 2 4, 4 2, 6 5, 6 7, 5 6, 7 6)"),
        from_wkt<MPo>("MULTIPOLYGON(((0 0, 0 5, 5 5, 5 0, 0 0),(0 0, 4 1, 4 4, 1 4, 0 0)),"
                                   "((5 5, 5 7, 7 7, 7 5, 5 5)))"),
        from_wkt<MPt>("MULTIPOINT(3 6, 6 3, 3 -1, -1 3)")
    );
}
