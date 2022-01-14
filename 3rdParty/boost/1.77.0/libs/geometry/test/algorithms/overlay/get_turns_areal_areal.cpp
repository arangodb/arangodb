// Boost.Geometry
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2015.
// Modifications copyright (c) 2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_get_turns.hpp"
#include <boost/geometry/geometries/geometries.hpp>

//TEST
//#include <to_svg.hpp>

template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::cartesian> pt;
    //typedef bg::model::ring<pt> ring;
    typedef bg::model::polygon<pt> poly;
    //typedef bg::model::multi_polygon<polygon> mpoly;

    // mailing list report 17.03.2015
    // operations ok but wrong IPs for int
    // (the coordinates are generates at endpoints only)
    {
        // cw(duplicated point)
        test_geometry<poly, poly>("POLYGON((-8042 -1485,-8042 250,-8042 250,15943 254,15943 -1485,-8042 -1485))",
                                  "POLYGON((-7901 -1485,-7901 529,-7901 529, 15802 544, 15802 -1485, -7901 -1485))",
                                  expected("iiu")("iui")("mcc")("cui"));
        //to_svg<poly, poly>("POLYGON((-8042 -1485,-8042 250,15943 254,15943 -1485,-8042 -1485))",
        //                   "POLYGON((-7901 -1485,-7901 529,15802 544, 15802 -1485, -7901 -1485))",
        //                   "poly_poly_1.svg");
        test_geometry<poly, poly>("POLYGON((-7901 -1485,-7901 529,-7901 529, 15802 544, 15802 -1485, -7901 -1485))",
                                  "POLYGON((-8042 -1485,-8042 250,-8042 250,15943 254,15943 -1485,-8042 -1485))",
                                  expected("iui")("iiu")("mcc")("ciu"));
    }
}

int test_main(int, char* [])
{
    test_all<int>();
    test_all<float>();
    test_all<double>();

#if ! defined(_MSC_VER)
    test_all<long double>();
#endif

    return 0;
}
