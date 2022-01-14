// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_get_turns.hpp"
#include <boost/geometry/geometries/geometries.hpp>

#include <algorithms/overlay/overlay_cases.hpp>


template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::spherical_equatorial<bg::degree> > pt;
    //typedef bg::model::ring<pt> ring;
    typedef bg::model::polygon<pt> poly;
    //typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<poly, poly>(case_1[0], case_1[1],
                              expected("iiu")("iui")("iiu")("iui")("iiu")("iui"));
    test_geometry<poly, poly>(case_2[0], case_2[1],
                              expected("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu"));
    test_geometry<poly, poly>(case_3_sph[0], case_3_sph[1],
                              expected("miu")("miu")("miu")("miu"));
    test_geometry<poly, poly>(case_4[0], case_4[1],
                              expected("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui"));
    test_geometry<poly, poly>(case_5[0], case_5[1],
                              expected("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui"));
    test_geometry<poly, poly>(case_6_sph[0], case_6_sph[1],
                              expected("ccc")("eui")("mcc"));

    test_geometry<poly, poly>(case_7[0], case_7[1],
                              expected("txu")("tux"));
    test_geometry<poly, poly>(case_8_sph[0], case_8_sph[1],
                              expected("mux")("cxu"));
    test_geometry<poly, poly>(case_9_sph[0], case_9_sph[1],
                              expected("muu"));
    test_geometry<poly, poly>(case_10_sph[0], case_10_sph[1],
                              expected("cxu")("mux")("txx"));
    test_geometry<poly, poly>(case_11_sph[0], case_11_sph[1],
                              expected("mui"));
    test_geometry<poly, poly>(case_12[0], case_12[1],
                              expected("iiu")("iui")("iiu")("iui")("iiu")("iui")("iiu")("iui"));

    test_geometry<poly, poly>(case_13_sph[0], case_13_sph[1],
                              expected("mxu")("mux"));
    test_geometry<poly, poly>(case_14_sph[0], case_14_sph[1],
                              expected("cxu")("mux"));
    test_geometry<poly, poly>(case_15_sph[0], case_15_sph[1],
                              expected("cxu")("mux"));
    test_geometry<poly, poly>(case_16_sph[0], case_16_sph[1],
                              expected("txx")("txx")("tux")("cxu"));
    test_geometry<poly, poly>(case_17_sph[0], case_17_sph[1],
                              expected("mcc")("cui"));
    test_geometry<poly, poly>(case_18_sph[0], case_18_sph[1],
                              expected("mcc")("ccc")("ccc")("cui"));

    test_geometry<poly, poly>("POLYGON((16 15,-132 10,-56 89,67 5,16 15))",
                              "POLYGON((101 49,12 40,-164 10,117 0,101 49))",
                              expected("iiu")("iui"));
}

int test_main(int, char* [])
{
    test_all<float>();
    test_all<double>();

//#if ! defined(_MSC_VER)
//    test_all<long double>();
//#endif

    return 0;
}
