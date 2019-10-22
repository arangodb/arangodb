// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2015, 2016.
// Modifications copyright (c) 2015-2016, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>

#include <boost/geometry/geometries/point_xy.hpp>

#include "test_difference.hpp"
#include <algorithms/test_overlay.hpp>
#include <algorithms/overlay/overlay_cases.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>


#ifdef HAVE_TTMATH
#  include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif


// Convenience macros (points are not checked)
#define TEST_DIFFERENCE(caseid, clips1, area1, clips2, area2, clips3) \
    (test_one<polygon, polygon, polygon>) \
    ( #caseid, caseid[0], caseid[1], clips1, -1, area1, clips2, -1, area2, \
                clips3, -1, area1 + area2)

#define TEST_DIFFERENCE_WITH(caseid, clips1, area1, clips2, area2, clips3) \
    (test_one<polygon, polygon, polygon>) \
    ( #caseid, caseid[0], caseid[1], clips1, -1, area1, clips2, -1, area2, \
                clips3, -1, area1 + area2, settings)

template <typename P>
void test_all()
{
    typedef bg::model::box<P> box;
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::ring<P> ring;

    typedef typename bg::coordinate_type<P>::type ct;

    ut_settings sym_settings;
#if ! defined(BOOST_GEOMETRY_USE_RESCALING)
    sym_settings.sym_difference = false;
#endif

    ut_settings ignore_validity_settings;
    ignore_validity_settings.test_validity = false;

    test_one<polygon, polygon, polygon>("simplex_normal",
        simplex_normal[0], simplex_normal[1],
        3, 12, 2.52636706856656,
        3, 12, 3.52636706856656,
        sym_settings);

    test_one<polygon, polygon, polygon>("simplex_with_empty",
        simplex_normal[0], polygon_empty,
        1, 4, 8.0,
        0, 0, 0.0);

    test_one<polygon, polygon, polygon>(
            "star_ring", example_star, example_ring,
            5, 22, 1.1901714,
            5, 27, 1.6701714,
            sym_settings);

    test_one<polygon, polygon, polygon>("two_bends",
        two_bends[0], two_bends[1],
        1, 5, 8.0,
        1, 5, 8.0);

    test_one<polygon, polygon, polygon>("star_comb_15",
        star_comb_15[0], star_comb_15[1],
        30, -1, 227.658275102812,
        30, -1, 480.485775259312,
        sym_settings);

    test_one<polygon, polygon, polygon>("new_hole",
        new_hole[0], new_hole[1],
        1, 9, 7.0,
        1, 13, 14.0);


    test_one<polygon, polygon, polygon>("crossed",
        crossed[0], crossed[1],
        1, 18, 19.5,
        1, 7, 2.5);

    test_one<polygon, polygon, polygon>("disjoint",
        disjoint[0], disjoint[1],
        1, 5, 1.0,
        1, 5, 1.0);

    // The too small one might be discarded (depending on point-type / compiler)
    // We check area only
    test_one<polygon, polygon, polygon>("distance_zero",
        distance_zero[0], distance_zero[1],
        -1, -1, 8.7048386,
        -1, -1, 0.0098387,
        tolerance(0.001));

    test_one<polygon, polygon, polygon>("equal_holes_disjoint",
        equal_holes_disjoint[0], equal_holes_disjoint[1],
        1, 5, 9.0,
        1, 5, 9.0);

    test_one<polygon, polygon, polygon>("only_hole_intersections1",
        only_hole_intersections[0], only_hole_intersections[1],
        2, 10,  1.9090909,
        4, 16, 10.9090909,
        sym_settings);

    test_one<polygon, polygon, polygon>("only_hole_intersection2",
        only_hole_intersections[0], only_hole_intersections[2],
        3, 20, 30.9090909,
        4, 16, 10.9090909,
        sym_settings);

    test_one<polygon, polygon, polygon>("first_within_second",
        first_within_second[1], first_within_second[0],
        1, 10, 24,
        0, 0, 0);

    test_one<polygon, polygon, polygon>("fitting",
        fitting[0], fitting[1],
        1, 9, 21.0,
        1, 4, 4.0,
        1, 5, 25.0);

    test_one<polygon, polygon, polygon>("identical",
        identical[0], identical[1],
        0, 0, 0.0,
        0, 0, 0.0);

    test_one<polygon, polygon, polygon>("intersect_exterior_and_interiors_winded",
        intersect_exterior_and_interiors_winded[0], intersect_exterior_and_interiors_winded[1],
        4, 20, 11.533333,
        5, 26, 29.783333);

    test_one<polygon, polygon, polygon>("intersect_holes_intersect_and_disjoint",
        intersect_holes_intersect_and_disjoint[0], intersect_holes_intersect_and_disjoint[1],
        2, 16, 15.75,
        3, 17, 6.75,
        ignore_validity_settings);

    test_one<polygon, polygon, polygon>("intersect_holes_intersect_and_touch",
        intersect_holes_intersect_and_touch[0], intersect_holes_intersect_and_touch[1],
        3, 21, 16.25,
        3, 17, 6.25,
        ignore_validity_settings);

    {
        ut_settings settings = sym_settings;
        settings.percentage = 0.01;
        test_one<polygon, polygon, polygon>("intersect_holes_new_ring",
            intersect_holes_new_ring[0], intersect_holes_new_ring[1],
            3, 15, 9.8961,
            4, 25, 121.8961,
            settings);
    }

    test_one<polygon, polygon, polygon>("first_within_hole_of_second",
        first_within_hole_of_second[0], first_within_hole_of_second[1],
        1, 5, 1,
        1, 10, 16);

    test_one<polygon, polygon, polygon>("intersect_holes_disjoint",
        intersect_holes_disjoint[0], intersect_holes_disjoint[1],
        2, 14, 16.0,
        2, 10, 6.0);

    test_one<polygon, polygon, polygon>("intersect_holes_intersect",
        intersect_holes_intersect[0], intersect_holes_intersect[1],
        2, 16, 15.75,
        2, 12, 5.75,
        ignore_validity_settings);

    test_one<polygon, polygon, polygon>(
            "case4", case_4[0], case_4[1],
            6, 28, 2.77878787878788,
            4, 22, 4.77878787878788,
            sym_settings);

    test_one<polygon, polygon, polygon>(
            "case5", case_5[0], case_5[1],
            8, 36, 2.43452380952381,
            7, 33, 3.18452380952381);

#if ! defined(BOOST_GEOMETRY_USE_RESCALING)
    // Fails, a-b is partly generated, b-a does not have any output
    // It failed already in 1.59
    test_one<polygon, polygon, polygon>("case_58_iet",
        case_58[0], case_58[2],
        3, 12, 0.6666666667,
        1, -1, 11.1666666667,
        2, -1, 0.6666666667 + 11.1666666667);
#endif

    test_one<polygon, polygon, polygon>("case_80",
        case_80[0], case_80[1],
        1, 9, 44.5,
        1, 10, 84.5);

#if ! defined(BOOST_GEOMETRY_USE_RESCALING)
    // Fails, holes are not subtracted
    test_one<polygon, polygon, polygon>("case_81",
        case_81[0], case_81[1],
        1, 8, 80.5,
        1, 8, 83.0,
        1, 12, 80.5 + 83.0);
#endif

    test_one<polygon, polygon, polygon>("case_100",
        case_100[0], case_100[1],
        1, 7, 3.125,
        1, 7, 16.0,
        1, 13, 16.0 + 3.125);

    test_one<polygon, polygon, polygon>("case_101",
        case_101[0], case_101[1],
        3, 17, 13.75,
        1, 4, 1.0);

    test_one<polygon, polygon, polygon>("case_102",
        case_102[0], case_102[1],
        4, 18, 1.5,
        3, 15, 4.0625);

    TEST_DIFFERENCE(case_105, 4, 8.0, 1, 16.0, 5);
    TEST_DIFFERENCE(case_106, 1, 17.5, 2, 32.5, 3);
    TEST_DIFFERENCE(case_107, 2, 18.0, 2, 29.0, 4);

    test_one<polygon, polygon, polygon>("winded",
        winded[0], winded[1],
        3, 37, 61,
        1, 15, 13);

    test_one<polygon, polygon, polygon>("within_holes_disjoint",
        within_holes_disjoint[0], within_holes_disjoint[1],
        2, 15, 25,
        1, 5, 1);

    test_one<polygon, polygon, polygon>("side_side",
        side_side[0], side_side[1],
        1, 5, 1,
        1, 5, 1,
        1, 7, 2);

    test_one<polygon, polygon, polygon>("buffer_mp1",
        buffer_mp1[0], buffer_mp1[1],
        1, 61, 10.2717,
        1, 61, 10.2717);

    if ( BOOST_GEOMETRY_CONDITION((boost::is_same<ct, double>::value)) )
    {
        test_one<polygon, polygon, polygon>("buffer_mp2",
            buffer_mp2[0], buffer_mp2[1],
            1, 91, 12.09857,
            1, 155, 24.19714,
            BG_IF_RESCALED(2, 1), -1, 12.09857 + 24.19714);
    }

    /*** TODO: self-tangencies for difference
    test_one<polygon, polygon, polygon>("wrapped_a",
        wrapped[0], wrapped[1],
        3, 1, 61,
        1, 0, 13);

    test_one<polygon, polygon, polygon>("wrapped_b",
        wrapped[0], wrapped[2],
        3, 1, 61,
        1, 0, 13);
    ***/

    {
        ut_settings settings;
        settings.percentage = BG_IF_RESCALED(0.001, 0.1);
        settings.test_validity = BG_IF_RESCALED(true, false);
        settings.sym_difference = BG_IF_RESCALED(true, false);

        // Isovist - the # output polygons differ per compiler/pointtype, (very) small
        // rings might be discarded. We check area only

        // SQL Server gives:    0.279121891701124 and 224.889211358929
        // PostGIS gives:       0.279121991127244 and 224.889205853156
        // No robustness gives: 0.279121991127106 and 224.825363749290

        test_one<polygon, polygon, polygon>("isovist",
            isovist1[0], isovist1[1],
            -1, -1, 0.279132,
            -1, -1, 224.8892,
            settings);
    }

#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    test_one<polygon, polygon, polygon>("geos_1",
        geos_1[0], geos_1[1],
        21, -1, 0.31640625,
         9, -1, 0.01953125);

    // Excluded this test in the normal suite, it is OK like this for many clang/gcc/msvc
    // versions, but NOT OK for many other clang/gcc/msvc versions on other platforms
    // It might depend on partition (order)
    //        10, -1, 0.02148439); // change in partition might give these results

    // SQL Server gives: 0.28937764436705 and 0.000786406897532288 with 44/35 rings
    // PostGIS gives:    0.30859375       and 0.033203125 with 35/35 rings
#endif

    {
        // MSVC 14 expects 138.69214 and 211.85913: increase percentage

        ut_settings settings = sym_settings;
        settings.percentage = 0.01;
        settings.test_validity = false;

        // Output polygons for sym difference might be combined
        test_one<polygon, polygon, polygon>("geos_2",
            geos_2[0], geos_2[1],
            1, -1, 138.6923828,
            1, -1, 211.859375,
            BG_IF_RESCALED(2, 1), -1, 138.6923828 + 211.859375,
            settings);
    }

    // Output polygons for sym difference might be combined
    test_one<polygon, polygon, polygon>("geos_3",
        geos_3[0], geos_3[1],
        1, -1, 16211128.5,
        1, -1, 13180420.0,
        BG_IF_RESCALED(1, 2), -1, 16211128.5 + 13180420.0,
        sym_settings);

    test_one<polygon, polygon, polygon>("geos_4",
        geos_4[0], geos_4[1],
        1, -1, 971.9163115,
        1, -1, 1332.4163115,
        sym_settings);

    test_one<polygon, polygon, polygon>("ggl_list_20110306_javier",
        ggl_list_20110306_javier[0], ggl_list_20110306_javier[1],
        1, -1, 71495.3331,
        2, -1, 8960.49049,
        2, -1, 71495.3331 + 8960.49049);

    test_one<polygon, polygon, polygon>("ggl_list_20110307_javier",
        ggl_list_20110307_javier[0], ggl_list_20110307_javier[1],
        1, if_typed<ct, float>(14, 13), 16815.6,
        1, 4, 3200.4,
        tolerance(0.01));

    if ( BOOST_GEOMETRY_CONDITION((! boost::is_same<ct, float>::value)) )
    {
        test_one<polygon, polygon, polygon>("ggl_list_20110716_enrico",
            ggl_list_20110716_enrico[0], ggl_list_20110716_enrico[1],
            3, -1, 35723.8506317139,
            1, -1, 58456.4964294434,
            1, -1, 35723.8506317139 + 58456.4964294434);
    }

#if defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    {
        // symmetric difference is not valid due to robustness issue, it has
        // two turns (touch_only) and a midpoint is located in other polygon
        ut_settings ignore_validity;
        ignore_validity.test_validity = false;

        test_one<polygon, polygon, polygon>("ggl_list_20110820_christophe",
            ggl_list_20110820_christophe[0], ggl_list_20110820_christophe[1],
            1, -1, 2.8570121719168924,
            1, -1, 64.498061986388564,
                ignore_validity);
    }
#endif

    test_one<polygon, polygon, polygon>("ggl_list_20120717_volker",
        ggl_list_20120717_volker[0], ggl_list_20120717_volker[1],
        1, 11, 3370866.2295081965,
        1, 5, 384.2295081964694,
        tolerance(0.01));

    // 2011-07-02 / 2014-06-19
    // Interesting FP-precision case.
    // sql server gives: 6.62295817619452E-05
    // PostGIS gives: 0.0 (no output)
    // Boost.Geometry gave results depending on FP-type, and compiler, and operating system.
    // With rescaling results are equal w.r.t. compiler/FP type,
    // however, some long spikes are still generated in the resulting difference
    // Without rescaling there is no output, like PostGIS
    test_one<polygon, polygon, polygon>("ggl_list_20110627_phillip",
        ggl_list_20110627_phillip[0], ggl_list_20110627_phillip[1],
        BG_IF_RESCALED(1, 0), -1,
        BG_IF_RESCALED(if_typed_tt<ct>(0.0000000000001105367, 0.000125137888971949), 0),
        1, -1, 3577.40960816756,
        tolerance(0.01)
        );

    {
        // With rescaling, difference of output a-b and a sym b is invalid
        ut_settings settings;
        settings.test_validity = BG_IF_RESCALED(false, true);
        TEST_DIFFERENCE_WITH(ggl_list_20190307_matthieu_1, 2, 0.18461532, 2, 0.617978, 4);
        TEST_DIFFERENCE_WITH(ggl_list_20190307_matthieu_2, 2, 12.357152, 0, 0.0, 2);
    }

    // Ticket 8310, one should be completely subtracted from the other.
    test_one<polygon, polygon, polygon>("ticket_8310a",
        ticket_8310a[0], ticket_8310a[1],
        1, 10, 10.11562724,
        0, 0, 0);
    test_one<polygon, polygon, polygon>("ticket_8310b",
        ticket_8310b[0], ticket_8310b[1],
        1, 10, 10.12655608,
        0, 0, 0);
    test_one<polygon, polygon, polygon>("ticket_8310c",
        ticket_8310c[0], ticket_8310c[1],
        1, 10, 10.03103292,
        0, 0, 0);

    test_one<polygon, polygon, polygon>("ticket_9081_15",
            ticket_9081_15[0], ticket_9081_15[1],
            2, -1, 0.0334529710902111,
            BG_IF_RESCALED(1, 0), -1, BG_IF_RESCALED(5.3469555172380723e-010, 0));

    test_one<polygon, polygon, polygon>("ticket_9081_314",
            ticket_9081_314[0], ticket_9081_314[1],
            2, 12, 0.0451236449624935,
            0, 0, 0);

#if defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    test_one<polygon, polygon, polygon>("ticket_9563",
            ticket_9563[0], ticket_9563[1],
            0, -1, 0,
            6, -1, 20.096189);
#endif

    test_one<polygon, polygon, polygon>("ticket_10108_a",
            ticket_10108_a[0], ticket_10108_a[1],
            1, 4,  0.0145037,
            1, 4,  0.029019232,
            sym_settings);

    test_one<polygon, polygon, polygon>("ticket_10108_b",
            ticket_10108_b[0], ticket_10108_b[1],
            1, -1, 1081.68697,
            1, -1, 1342.65795,
            BG_IF_RESCALED(2, 1), -1, 1081.68697 + 1342.65795);

    test_one<polygon, polygon, polygon>("ticket_11725",
        ticket_11725[0], ticket_11725[1],
        1, -1, 3.0,
        1, -1, 4.5,
        1, -1, 7.5);

    // From assemble-test, with a u/u case
    test_one<polygon, polygon, polygon>("assemble_0210",
            "POLYGON((0 0,0 10,10 10,10 0,0 0),(8.5 1,9.5 1,9.5 2,8.5 2,8.5 1))",
            "POLYGON((2 0.5,0.5 2,0.5 8,2 9.5,6 9.5,8.5 8,8.5 2,7 0.5,2 0.5),(2 2,7 2,7 8,2 8,2 2))",
            2, 23, 62.25,
            0, 0, 0.0);

    // Other combi's
    {
        test_one<polygon, polygon, ring>(
                "star_ring_ring", example_star, example_ring,
                5, 22, 1.1901714,
                5, 27, 1.6701714,
                sym_settings);

        test_one<polygon, ring, polygon>(
                "ring_star_ring", example_ring, example_star,
                5, 27, 1.6701714,
                5, 22, 1.1901714,
                sym_settings);

        static std::string const clip = "POLYGON((2.5 0.5,5.5 2.5))";

        test_one<polygon, box, ring>("star_box",
            clip, example_star,
            4, 20, 2.833333, 4, 16, 0.833333);

        test_one<polygon, ring, box>("box_star",
            example_star, clip,
            4, 16, 0.833333, 4, 20, 2.833333);
    }

    // Counter clockwise
    {
        typedef bg::model::polygon<P, false> polygon_ccw;
        test_one<polygon, polygon_ccw, polygon_ccw>(
                "star_ring_ccw", example_star, example_ring,
                5, 22, 1.1901714,
                5, 27, 1.6701714,
                sym_settings);
        test_one<polygon, polygon, polygon_ccw>(
                "star_ring_ccw1", example_star, example_ring,
                5, 22, 1.1901714,
                5, 27, 1.6701714,
                sym_settings);
        test_one<polygon, polygon_ccw, polygon>(
                "star_ring_ccw2", example_star, example_ring,
                5, 22, 1.1901714,
                5, 27, 1.6701714,
                sym_settings);
    }

    // Multi/box (should be moved to multi)
    {
        typedef bg::model::multi_polygon<polygon> mp;

        static std::string const clip = "POLYGON((2 2,4 4))";

        test_one<polygon, box, mp>("simplex_multi_box_mp",
            clip, case_multi_simplex[0],
            2, -1, 0.53333333333, 3, -1, 8.53333333333);
        test_one<polygon, mp, box>("simplex_multi_mp_box",
            case_multi_simplex[0], clip,
            3, -1, 8.53333333333, 2, -1, 0.53333333333);

    }

    // Rescaling generates a very small false polygon
    TEST_DIFFERENCE(issue_566_a, 1, 143.662, BG_IF_RESCALED(1, 0),
                    BG_IF_RESCALED(1.605078e-6, 0.0),
                    BG_IF_RESCALED(2, 1));
    TEST_DIFFERENCE(issue_566_b, 1, 143.662, BG_IF_RESCALED(1, 0),
                    BG_IF_RESCALED(1.605078e-6, 0.0),
                    BG_IF_RESCALED(2, 1));

    /***
    Experimental (cut), does not work:
    test_one<polygon, polygon, polygon>(
            "polygon_pseudo_line",
            "POLYGON((0 0,0 4,4 4,4 0,0 0))",
            "POLYGON((2 -2,2 -1,2 6,2 -2))",
            5, 22, 1.1901714,
            5, 27, 1.6701714);
    ***/

    TEST_DIFFERENCE(mysql_21977775, 2, 160.856568913, 2, 92.3565689126, 4);
    TEST_DIFFERENCE(mysql_21965285, 1, 92.0, 1, 14.0, 1);
    TEST_DIFFERENCE(mysql_23023665_1, 1, 92.0, 1, 142.5, 2);
    TEST_DIFFERENCE(mysql_23023665_2, 1, 96.0, 1, 16.0, 2);
    TEST_DIFFERENCE(mysql_23023665_3, 1, 225.0, 1, 66.0, 2);
    TEST_DIFFERENCE(mysql_23023665_5, 2, 165.23735, 2, 105.73735, 4);
#if defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    TEST_DIFFERENCE(mysql_23023665_6, 2, 105.68756, 3, 10.18756, 5);
    TEST_DIFFERENCE(mysql_23023665_13, 3, 99.74526, 3, 37.74526, 6);
#endif
}


// Test cases for integer coordinates / ccw / open
template <typename Point, bool ClockWise, bool Closed>
void test_specific()
{
    typedef bg::model::polygon<Point, ClockWise, Closed> polygon;

    test_one<polygon, polygon, polygon>("ggl_list_20120717_volker",
        ggl_list_20120717_volker[0], ggl_list_20120717_volker[1],
        1, 11, 3371540,
        1, 4, 385,
        1, 16, 3371540 + 385);

    test_one<polygon, polygon, polygon>("ticket_10658",
        ticket_10658[0], ticket_10658[1],
        1, 6, 1510434,
        0, 0, 0);

    test_one<polygon, polygon, polygon>("ticket_11121",
        ticket_11121[0], ticket_11121[1],
        2, 8, 489763.5,
        1, 4, 6731652.0);

    // Generates spikes, both a-b and b-a
    TEST_DIFFERENCE(ticket_11676, 2, 2537992.5, 2, 294963.5, 3);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double> >();

    test_specific<bg::model::d2::point_xy<int>, false, false>();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<bg::model::d2::point_xy<float> >();

#ifdef HAVE_TTMATH
    std::cout << "Testing TTMATH" << std::endl;
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif
#endif

    return 0;
}
