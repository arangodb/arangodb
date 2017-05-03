// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2016.
// Modifications copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>


#include <geometry_test_common.hpp>

#include "test_intersects.hpp"

#include <boost/geometry.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

template <typename P1, typename P2>
void test_all()
{
    typedef bg::model::polygon<P1> polygon1;
    typedef bg::model::multi_polygon<polygon1> mp1;
    typedef bg::model::polygon<P2> polygon2;
    typedef bg::model::multi_polygon<polygon2> mp2;

    test_geometry<mp1, mp2>("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
            "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
        true);

    test_geometry<P1, mp2>("POINT(0 0)",
        "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
        true);

}

template <typename P>
void test_all()
{
    test_all<P, P>();
}

int test_main(int, char* [])
{
    //test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();
    test_all<bg::model::d2::point_xy<double>, bg::model::point<double, 2, bg::cs::cartesian> >();

#ifdef HAVE_TTMATH
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}

