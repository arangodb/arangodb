// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2016 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithms/test_perimeter.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

template <typename P>
void test_all()
{
    // Simple
    test_geometry<bg::model::polygon<P> >("POLYGON((0 0,3 4,4 3,0 0))",
                                          5 + sqrt(2.0) + 5);
    // Non-simple
    test_geometry<bg::model::polygon<P> >("POLYGON((0 0,3 4,4 3,0 3,0 0))",
                                          5 + sqrt(2.0) + 4 + 3);
    // With holes
    test_geometry<bg::model::polygon<P> >("POLYGON((0 0,3 4,4 3,0 0),\
                                                   (2 2,3 4,3 3,2 2))",
                                          5 + sqrt(2.0) + 5 +
                                          sqrt(5.0) + 1 + sqrt(2.0));
    // Repeated points
    test_geometry<bg::model::polygon<P> >("POLYGON((0 0,3 4,3 4,3 4,4 3,4 3,\
                                                    4 3,4 3,4 3,4 3,0 3,0 0))",
                                          5 + sqrt(2.0) + 4 + 3);
    // Multipolygon
    test_geometry<bg::model::multi_polygon<bg::model::polygon<P> > >
    (
        "MULTIPOLYGON(((0 0,3 4,4 3,0 0)), ((0 0,3 4,4 3,0 3,0 0)))",
        5 + sqrt(2.0) + 5 + 5 + sqrt(2.0) + 4 + 3
    );

    // Geometries with perimeter zero
    test_geometry<P>("POINT(0 0)", 0);
    test_geometry<bg::model::linestring<P> >("LINESTRING(0 0,3 4,4 3)", 0);
}

template <typename P>
void test_empty_input()
{
    test_empty_input(bg::model::polygon<P>());
    test_empty_input(bg::model::multi_polygon<bg::model::polygon<P> >());
}

int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();

#if defined(HAVE_TTMATH)
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    // test_empty_input<bg::model::d2::point_xy<int> >();

    return 0;
}
