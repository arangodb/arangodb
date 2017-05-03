// Boost.Geometry

// Copyright (c) 2016 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_covered_by.hpp"

#include <algorithms/overlay/overlay_cases.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/geometries/geometries.hpp>


template <typename P>
void test_polygon_polygon()
{
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;

    test_geometry<ring, ring>(case_1[0], case_1[1],
                              false);
    test_geometry<ring, poly>(case_1[0], case_1[1],
                              false);

    test_geometry<poly, poly>(case_1[0], case_1[1],
                              false);
    test_geometry<poly, poly>(case_2[0], case_2[1],
                              false);
    test_geometry<poly, poly>(case_3_sph[0], case_3_sph[1],
                              true);
    test_geometry<poly, poly>(case_3_2_sph[0], case_3_2_sph[1],
                              true);
    test_geometry<poly, poly>(case_4[0], case_4[1],
                              false);
    test_geometry<poly, poly>(case_5[0], case_5[1],
                              false);
    test_geometry<poly, poly>(case_6_sph[0], case_6_sph[1],
                              false);
    test_geometry<poly, poly>(case_6_sph[1], case_6_sph[0],
                              true);

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
    test_geometry<poly, poly>(case_11_sph[1], case_11_sph[0],
                              true);
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
    test_geometry<poly, poly>(case_17_sph[1], case_17_sph[0],
                              true);
    test_geometry<poly, poly>(case_18_sph[0], case_18_sph[1],
                              false);
    test_geometry<poly, poly>(case_18_sph[1], case_18_sph[0],
                              true);
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
void test_linestring_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::polygon<P> ring;

    test_geometry<ls, poly>("LINESTRING(11 0,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_geometry<ls, ring>("LINESTRING(11 0,11 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);
    test_geometry<ls, poly>("LINESTRING(0 0,10 10)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", true);
    test_geometry<ls, poly>("LINESTRING(5 0,5 5,10 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", true);
    test_geometry<ls, poly>("LINESTRING(5 1,5 5,9 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", true);
    test_geometry<ls, poly>("LINESTRING(11 1,11 5)", "POLYGON((0 0,0 10,10 10,10 0,0 0))", false);

    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            true);

    test_geometry<ls, poly>("LINESTRING(9 1,10 5,9 9,1 9,1 1,9 1)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5))",
                            true);

    test_geometry<ls, poly>("LINESTRING(0 0,10 0,10 10,0 10,0 0)",
                            "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                            true);
}

template <typename P>
void test_linestring_multi_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<ls, mpoly>("LINESTRING(10 1,10 5,10 9)",
                             "MULTIPOLYGON(((0 20,0 30,10 30,10 20,0 20)),((0 0,0 10,10 10,10 0,0 0),(10 5,2 8,2 2,10 5)))",
                             true);
}

template <typename P>
void test_multi_linestring_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::ring<P> ring;
    typedef bg::model::multi_linestring<ls> mls;

    test_geometry<mls, poly>("MULTILINESTRING((11 11, 20 20),(5 7, 4 1))",
                             "POLYGON((0 0,0 10,10 10,10 0,0 0),(2 2,4 2,4 4,2 4,2 2))",
                             false);

    test_geometry<mls, ring>("MULTILINESTRING((6 6,15 15),(0 0, 7 7))",
                             "POLYGON((5 5,5 15,15 15,15 5,5 5))",
                             false);

    test_geometry<mls, poly>("MULTILINESTRING((3 10.031432746397092, 1 5, 1 10.013467818052765, 3 4, 7 8, 6 10.035925377760330, 10 2))",
                             "POLYGON((0 0,0 10,10 10,10 0,0 0))",
                             true);
}

template <typename P>
void test_multi_linestring_multi_polygon()
{
    typedef bg::model::linestring<P> ls;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::multi_polygon<poly> mpoly;

    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10,0 10,0 0),(2 2,5 5,2 8,2 2))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(2 2,5 5,2 8,2 2)))",
                              true);

    test_geometry<mls, mpoly>("MULTILINESTRING((0 0,10 0,10 10),(10 10,0 10,0 0),(20 20,50 50,20 80,20 20))",
                              "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)))",
                              false);

    test_geometry<mls, mpoly>("MULTILINESTRING((5 -2,4 -2,5 0),(5 -2,6 -2,5 0))",
                              "MULTIPOLYGON(((5 0,0 5,10 5,5 0)),((5 0,10 -5,0 -5,5 0)))",
                              true);
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
void test_point_polygon()
{
    typedef bg::model::polygon<P> poly;
    
    // https://svn.boost.org/trac/boost/ticket/9162
    test_geometry<P, poly>("POINT(0 90)",
                           "POLYGON((0 80,-90 80, -180 80, 90 80, 0 80))",
                           true);
    test_geometry<P, poly>("POINT(-120 21)",
                           "POLYGON((30 0,30 30,90 30, 90 0, 30 0))",
                           false);
    // extended
    test_geometry<P, poly>("POINT(0 -90)",
                           "POLYGON((0 -80,90 -80, -180 -80, -90 -80, 0 -80))",
                           true);
    test_geometry<P, poly>("POINT(0 89)",
                           "POLYGON((0 80,-90 80, -180 80, 90 80, 0 80))",
                           true);
    test_geometry<P, poly>("POINT(-180 89)",
                           "POLYGON((0 80,-90 80, -180 80, 90 80, 0 80))",
                           true);
}


template <typename P>
void test_all()
{
    test_polygon_polygon<P>();
    test_polygon_multi_polygon<P>();
    test_multi_polygon_multi_polygon<P>();

    test_linestring_polygon<P>();
    test_linestring_multi_polygon<P>();
    test_multi_linestring_polygon<P>();
    test_multi_linestring_multi_polygon<P>();

    test_linestring_linestring<P>();
    test_linestring_multi_linestring<P>();
    test_multi_linestring_multi_linestring<P>();

    test_point_polygon<P>();
}


int test_main( int , char* [] )
{
    test_all<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();

#if defined(HAVE_TTMATH)
    test_cs<bg::model::point<ttmath_big, 2, bg::cs::spherical_equatorial<bg::degree> > >();
#endif

    return 0;
}
