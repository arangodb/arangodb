// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithms/test_convert.hpp>


template <typename Point1, typename Point2>
void test_mixed_point_types()
{
    check<bg::model::multi_point<Point1>, bg::model::multi_point<Point2>>(
        "MULTIPOINT((1 1),(2 2),(3 3))");

    check
        <
            bg::model::multi_linestring<bg::model::linestring<Point1> >,
            bg::model::multi_linestring<bg::model::linestring<Point2> >
        >("MULTILINESTRING((1 1,2 2),(3 3,4 4))");

    // Single -> multi (always possible)
    check<Point1, bg::model::multi_point<Point2>>(
        "POINT(1 1)", "MULTIPOINT((1 1))", 1);

    check
        <
            bg::model::linestring<Point1>,
            bg::model::multi_linestring<bg::model::linestring<Point2> >
        >("LINESTRING(1 1,2 2)", "MULTILINESTRING((1 1,2 2))", 2);

    check
        <
            bg::model::segment<Point1>,
            bg::model::multi_linestring<bg::model::linestring<Point2> >
        >("LINESTRING(1 1,2 2)", "MULTILINESTRING((1 1,2 2))", 2);

    check
        <
            bg::model::box<Point1>,
            bg::model::multi_polygon<bg::model::polygon<Point2> >
        >("BOX(0 0,1 1)", "MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0)))", 5);

    check
        <
            bg::model::ring<Point1, true>,
            bg::model::multi_polygon<bg::model::polygon<Point2, false> >
        >("POLYGON((0 0,0 1,1 1,1 0,0 0))", "MULTIPOLYGON(((0 0,1 0,1 1,0 1,0 0)))", 5);

    check
        <
            bg::model::ring<Point1, false, false>,
            bg::model::multi_polygon<bg::model::polygon<Point2> >
        >("POLYGON((0 0,1 0,1 1,0 1))", "MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0)))", 5);

    // Multi -> single: should not compile (because multi often have 0 or >1 elements)
}

template <typename Point1, typename Point2>
void test_mixed_types()
{
    test_mixed_point_types<Point1, Point2>();
    test_mixed_point_types<Point2, Point1>();
}

int test_main( int , char* [] )
{
    test_mixed_types
        <
            bg::model::point<int, 2, bg::cs::cartesian>,
            bg::model::point<double, 2, bg::cs::cartesian>
        >();

    return 0;
}
