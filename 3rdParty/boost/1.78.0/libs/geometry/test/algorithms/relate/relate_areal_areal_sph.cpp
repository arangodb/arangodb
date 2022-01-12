// Boost.Geometry
// Unit Test

// Copyright (c) 2016, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include "test_relate.hpp"

#include <algorithms/overlay/overlay_cases.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>


template <typename P>
void test_polygon_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;

    test_geometry<ring, ring>(case_1[0], case_1[1],
                              "212101212");
    test_geometry<ring, poly>(case_1[0], case_1[1],
                              "212101212");

    test_geometry<poly, poly>(case_1[0], case_1[1],
                              "212101212");
    test_geometry<poly, poly>(case_2[0], case_2[1],
                              "212101212");
    test_geometry<poly, poly>(case_3_sph[0], case_3_sph[1],
                              "2FF10F212");
    test_geometry<poly, poly>(case_3_2_sph[0], case_3_2_sph[1],
                              "2FFF1FFF2");
    test_geometry<poly, poly>(case_4[0], case_4[1],
                              "212101212");
    test_geometry<poly, poly>(case_5[0], case_5[1],
                              "212101212");
    test_geometry<poly, poly>(case_6_sph[0], case_6_sph[1],
                              "212F11FF2");

    test_geometry<poly, poly>(case_7[0], case_7[1],
                              "FF2F11212");
    test_geometry<poly, poly>(case_8_sph[0], case_8_sph[1],
                              "FF2F11212");
    test_geometry<poly, poly>(case_9_sph[0], case_9_sph[1],
                              "FF2F01212");
    test_geometry<poly, poly>(case_10_sph[0], case_10_sph[1],
                              "FF2F11212");
    test_geometry<poly, poly>(case_11_sph[0], case_11_sph[1],
                              "212F01FF2");
    test_geometry<poly, poly>(case_12[0], case_12[1],
                              "212101212");

    test_geometry<poly, poly>(case_13_sph[0], case_13_sph[1],
                              "FF2F11212");
    test_geometry<poly, poly>(case_14_sph[0], case_14_sph[1],
                              "FF2F11212");
    test_geometry<poly, poly>(case_15_sph[0], case_15_sph[1],
                              "FF2F11212");
    test_geometry<poly, poly>(case_16_sph[0], case_16_sph[1],
                              "FF2F11212");
    test_geometry<poly, poly>(case_17_sph[0], case_17_sph[1],
                              "212F11FF2");
    test_geometry<poly, poly>(case_18_sph[0], case_18_sph[1],
                              "212F11FF2");
}

template <typename P>
void test_polygon_multi_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<ring, mpoly>(case_1[0], case_multi_2[0],
                               "212101212");
    test_geometry<poly, mpoly>(case_2[0], case_multi_2[0],
                               "212101212");
}

template <typename P>
void test_multi_polygon_multi_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<mpoly, mpoly>(case_multi_2[0], case_multi_2[1],
                                "212101212");
}


template <typename P>
void test_all()
{
    test_polygon_polygon<P>();
    test_polygon_multi_polygon<P>();
    test_multi_polygon_multi_polygon<P>();
}

int test_main( int , char* [] )
{
    typedef bg::cs::spherical_equatorial<bg::degree> cs_t;
    test_all<bg::model::point<float, 2, cs_t> >();
    test_all<bg::model::point<double, 2, cs_t> >();

    return 0;
}
