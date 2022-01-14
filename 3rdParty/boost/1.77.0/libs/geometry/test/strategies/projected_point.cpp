// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2014 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2014 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <strategies/test_projected_point.hpp>


template <typename P1, typename P2>
void test_all_2d()
{
    test_2d<P1, P2>("POINT(1 1)", "POINT(0 0)", "POINT(2 3)", 0.27735203958327);
    test_2d<P1, P2>("POINT(2 2)", "POINT(1 4)", "POINT(4 1)", 0.5 * sqrt(2.0));
    test_2d<P1, P2>("POINT(6 1)", "POINT(1 4)", "POINT(4 1)", 2.0);
    test_2d<P1, P2>("POINT(1 6)", "POINT(1 4)", "POINT(4 1)", 2.0);
}

template <typename P>
void test_all_2d()
{
    //test_all_2d<P, int[2]>();
    //test_all_2d<P, float[2]>();
    //test_all_2d<P, double[2]>();
    //test_all_2d<P, test::test_point>();
    test_all_2d<P, bg::model::point<int, 2, bg::cs::cartesian> >();
    test_all_2d<P, bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all_2d<P, bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all_2d<P, bg::model::point<long double, 2, bg::cs::cartesian> >();
}

int test_main(int, char* [])
{
    test_all_2d<int[2]>();
    test_all_2d<float[2]>();
    test_all_2d<double[2]>();
    //test_all_2d<test::test_point>();

    test_all_2d<bg::model::point<int, 2, bg::cs::cartesian> >();
    test_all_2d<bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all_2d<bg::model::point<double, 2, bg::cs::cartesian> >();

    test_services
        <
            bg::model::point<double, 2, bg::cs::cartesian>,
            bg::model::point<float, 2, bg::cs::cartesian>,
            long double
        >();

    return 0;
}
