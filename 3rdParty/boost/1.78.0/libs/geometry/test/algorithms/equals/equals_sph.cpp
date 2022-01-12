// Boost.Geometry

// Copyright (c) 2016 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_equals.hpp"

#include <algorithms/overlay/overlay_cases.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/geometries/geometries.hpp>


template <typename P>
void test_polygon_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;

    test_geometry<poly, ring>(case_1[0], case_1[0],
                              true);
    test_geometry<ring, ring>(case_1[0], case_1[1],
                              false);
    test_geometry<ring, poly>(case_1[0], case_1[1],
                              false);

    test_geometry<poly, poly>(case_1[0], case_1[1],
                              false);
    test_geometry<poly, poly>(case_2[0], case_2[1],
                              false);
    test_geometry<poly, poly>(case_3_sph[0], case_3_sph[1],
                              false);
    test_geometry<poly, poly>(case_3_2_sph[0], case_3_2_sph[1],
                              true);
    test_geometry<poly, poly>(case_4[0], case_4[1],
                              false);
    test_geometry<poly, poly>(case_5[0], case_5[1],
                              false);
    test_geometry<poly, poly>(case_6_sph[0], case_6_sph[1],
                              false);

    test_geometry<poly, poly>(case_7[0], case_7[1],
                              false);
    test_geometry<poly, poly>(case_8_sph[0], case_8_sph[1],
                              false);
    test_geometry<poly, poly>(case_9_sph[0], case_9_sph[1],
                              false);
    test_geometry<poly, poly>(case_10_sph[0], case_10_sph[1],
                              false);
    test_geometry<poly, poly>(case_11_sph[0], case_11_sph[1],
                              false);
    test_geometry<poly, poly>(case_12[0], case_12[1],
                              false);

    test_geometry<poly, poly>(case_13_sph[0], case_13_sph[1],
                              false);
    test_geometry<poly, poly>(case_14_sph[0], case_14_sph[1],
                              false);
    test_geometry<poly, poly>(case_15_sph[0], case_15_sph[1],
                              false);
    test_geometry<poly, poly>(case_16_sph[0], case_16_sph[1],
                              false);
    test_geometry<poly, poly>(case_17_sph[0], case_17_sph[1],
                              false);
    test_geometry<poly, poly>(case_18_sph[0], case_18_sph[1],
                              false);
}

template <typename P>
void test_polygon_multi_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<ring, mpoly>(case_1[0], case_multi_2[0],
                               false);
    test_geometry<poly, mpoly>(case_2[0], case_multi_2[0],
                               false);
}

template <typename P>
void test_multi_polygon_multi_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<mpoly, mpoly>(case_multi_2[0], case_multi_2[1],
                                false);
}

template <typename P>
void test_linestring_linestring()
{
    typedef bg::model::linestring<P> ls;

    test_geometry<ls, ls>("LINESTRING(0 0, 2 2, 3 2)", "LINESTRING(0 0, 2 2, 3 2)", true);

    test_geometry<ls, ls>("LINESTRING(1 0,2 2,2 3)", "LINESTRING(0 0, 2 2, 3 2)", false);
}

template <typename P>
void test_linestring_multi_linestring()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<ls, mls>("LINESTRING(0 0,10 0)",
                           "MULTILINESTRING((1 0,2 0),(1 1,2 1))",
                           false);

    test_geometry<ls, mls>("LINESTRING(0 0,5 0,5 5,0 5,0 0)",
                           "MULTILINESTRING((5 5,0 5,0 0),(0 0,5 0,5 5))",
                           true);
}

template <typename P>
void test_multi_linestring_multi_linestring()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<mls, mls>("MULTILINESTRING((0 0,0 0,18 0,18 0,19 0,19 0,19 0,30 0,30 0))",
                            "MULTILINESTRING((0 10,5 0,20 0,20 0,30 0))",
                            false);
}


template <typename P>
void test_all()
{
    test_polygon_polygon<P>();
    test_polygon_multi_polygon<P>();
    test_multi_polygon_multi_polygon<P>();

    test_linestring_linestring<P>();
    test_linestring_multi_linestring<P>();
    test_multi_linestring_multi_linestring<P>();
}


int test_main( int , char* [] )
{
    test_all<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();

    // TODO: the polar version needs several traits more, for example in cs_tag,
    // to compile properly.
    //test_all<bg::model::point<double, 2, bg::cs::polar<bg::degree> > >();

#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    test_all<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();
#endif

    return 0;
}
