// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

#include "test_difference.hpp"
#include <algorithms/test_overlay.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/intersection.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/io/wkt/read.hpp>

// Convenience macros (points are not checked)
#define TEST_DIFFERENCE(caseid, clips1, area1, clips2, area2, clips3) \
    (test_one<Polygon, MultiPolygon, MultiPolygon>) \
    ( #caseid, caseid[0], caseid[1], clips1, -1, area1, clips2, -1, area2, \
                clips3, -1, area1 + area2)

#define TEST_DIFFERENCE_IGNORE(caseid, clips1, area1, clips2, area2, clips3) \
    { ut_settings ignore_validity; ignore_validity.set_test_validity(false); \
    (test_one<Polygon, MultiPolygon, MultiPolygon>) \
    ( #caseid, caseid[0], caseid[1], clips1, -1, area1, clips2, -1, area2, \
                clips3, -1, area1 + area2, ignore_validity); }

#define TEST_DIFFERENCE_WITH(index1, index2, caseid, clips1, area1, \
                clips2, area2, clips3) \
    (test_one<Polygon, MultiPolygon, MultiPolygon>) \
    ( #caseid "_" #index1 "_" #index2, caseid[index1], caseid[index2], \
            clips1, -1, area1, \
            clips2, -1, area2, \
            clips3, -1, area1 + area2, settings)


template <typename Ring, typename Polygon, typename MultiPolygon>
void test_areal()
{
    test_one<Polygon, MultiPolygon, MultiPolygon>("simplex_multi",
            case_multi_simplex[0], case_multi_simplex[1],
            5, 21, 5.58, 4, 17, 2.58);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_no_ip",
            case_multi_no_ip[0], case_multi_no_ip[1],
            2, 12, 24.0, 2, 12, 34.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_2",
            case_multi_2[0], case_multi_2[1],
            2, 15, 19.6, 2, 13, 33.6);

    test_one<Polygon, MultiPolygon, Polygon>("simplex_multi_mp_p",
            case_multi_simplex[0], case_single_simplex,
            5, 21, 5.58, 4, 17, 2.58);
    test_one<Polygon, Ring, MultiPolygon>("simplex_multi_r_mp",
            case_single_simplex, case_multi_simplex[0],
            4, 17, 2.58, 5, 21, 5.58);
    test_one<Polygon, MultiPolygon, Ring>("simplex_multi_mp_r",
            case_multi_simplex[0], case_single_simplex,
            5, 21, 5.58, 4, 17, 2.58);

    // Constructed cases for multi/touch/equal/etc
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_61_multi",
            case_61_multi[0], case_61_multi[1],
            2, 10, 2, 2, 10, 2, 1, 10, 4);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_62_multi",
            case_62_multi[0], case_62_multi[1],
            0, 0, 0, 1, 5, 1);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_63_multi",
            case_63_multi[0], case_63_multi[1],
            0, 0, 0, 1, 5, 1);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_64_multi",
        case_64_multi[0], case_64_multi[1],
        1, 5, 1, 1, 5, 1, 1, 7, 2);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_65_multi",
        case_65_multi[0], case_65_multi[1],
            0, 0, 0, 2, 10, 3);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_72_multi",
        case_72_multi[0], case_72_multi[1],
            3, 13, 1.65, 3, 17, 6.15);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_77_multi",
        case_77_multi[0], case_77_multi[1],
            6, 31, 7.0,
            5, 33, 13.0,
            5, 38, 7.0 + 13.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_78_multi",
        case_78_multi[0], case_78_multi[1],
            1, 5, 1.0, 1, 5, 1.0);

    TEST_DIFFERENCE(case_123_multi, 1, 0.25, 2, 0.625, 3);
    TEST_DIFFERENCE(case_124_multi, 1, 0.25, 2, 0.4375, 3);
    TEST_DIFFERENCE(case_125_multi, 1, 0.25, 2, 0.400, 3);

    // A should have 3 clips, B should have 5 clips
    TEST_DIFFERENCE(case_126_multi, 4, 16.0, 5, 27.0, 9);

    {
        ut_settings settings;

        settings.sym_difference = BG_IF_RESCALED(false, true);

        test_one<Polygon, MultiPolygon, MultiPolygon>("case_108_multi",
            case_108_multi[0], case_108_multi[1],
                7, 32, 5.5,
                4, 24, 9.75,
                7, 45, 15.25,
                settings);
    }

    // Ticket on GGL list 2011/10/25
    // to mix polygon/multipolygon in call to difference
    test_one<Polygon, Polygon, Polygon>("ggl_list_20111025_vd_pp",
        ggl_list_20111025_vd[0], ggl_list_20111025_vd[1],
            1, 4, 8.0, 1, 4, 12.5);
    test_one<Polygon, Polygon, MultiPolygon>("ggl_list_20111025_vd_pm",
        ggl_list_20111025_vd[0], ggl_list_20111025_vd[3],
            1, 4, 8.0, 1, 4, 12.5);
    test_one<Polygon, MultiPolygon, Polygon>("ggl_list_20111025_vd_mp",
        ggl_list_20111025_vd[2], ggl_list_20111025_vd[1],
            1, 4, 8.0, 1, 4, 12.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20111025_vd_mm",
        ggl_list_20111025_vd[2], ggl_list_20111025_vd[3],
            1, 4, 8.0, 1, 4, 12.5);

    test_one<Polygon, Polygon, MultiPolygon>("ggl_list_20111025_vd_2",
        ggl_list_20111025_vd_2[0], ggl_list_20111025_vd_2[1],
            1, 7, 10.0, 2, 10, 6.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_a",
        ggl_list_20120915_h2[0], ggl_list_20120915_h2[1],
            2, 13, 17.0, 0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_b",
        ggl_list_20120915_h2[0], ggl_list_20120915_h2[2],
            2, 13, 17.0, 0, 0, 0.0);

    {
        ut_settings settings;
        settings.percentage = 0.001;
        settings.set_test_validity(BG_IF_RESCALED(true, false));
        TEST_DIFFERENCE_WITH(0, 1, ggl_list_20120221_volker, 2, 7962.66, 2, 2775258.93, 4);
    }

#if ! defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    {
        // 1: Very small sliver for B (discarded when rescaling)
        // 2: sym difference is not considered as valid (without rescaling
        //    this is a false negative)
        // 3: with rescaling A is considered as invalid (robustness problem)
        ut_settings settings;
        settings.validity_of_sym = BG_IF_RESCALED(false, true);
        settings.validity_false_negative_sym = true;
        TEST_DIFFERENCE_WITH(0, 1, bug_21155501,
                             (count_set(1, 4)), expectation_limits(3.75893, 3.75894),
                             (count_set(1, 4)), (expectation_limits(1.776357e-15, 7.661281e-15)),
                             (count_set(2, 5)));
    }
#endif

#if defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    {
        // With rescaling, it is complete but invalid
        // Without rescaling, one ring is missing (for a and s)
        ut_settings settings;
        settings.set_test_validity(BG_IF_RESCALED(false, true));
        settings.validity_of_sym = BG_IF_RESCALED(false, true);
        TEST_DIFFERENCE_WITH(0, 1, ticket_9081,
                             2, 0.0907392476356186,
                             4, 0.126018011439877,
                             count_set(3, 4));
    }
#endif

    TEST_DIFFERENCE(ticket_12503, 46, 920.625, 4, 7.625, 50);

    {
        // Reported issues going wrong with rescaling (except for 630b)
        ut_settings settings;
        settings.percentage = 0.001;

#if ! defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
        TEST_DIFFERENCE_WITH(0, 1, issue_630_a, 0, expectation_limits(0.0), 1, (expectation_limits(2.023, 2.2004)), 1);
#endif

        TEST_DIFFERENCE_WITH(0, 1, issue_630_b, 1, 0.0056089, 2, 1.498976, 3);

#if ! defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
        TEST_DIFFERENCE_WITH(0, 1, issue_630_c, 0, 0, 1, 1.493367, 1);
#endif

#if ! defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
        // Symmetrical difference fails without get_clusters
        settings.sym_difference = BG_IF_TEST_FAILURES;
        TEST_DIFFERENCE_WITH(0, 1, issue_643, 1, expectation_limits(76.5385), optional(), optional_sliver(1.0e-6), 1);
#endif
    }

    // Cases below go (or went) wrong in either a ( [0] - [1] ) or b ( [1] - [0] )
    // Requires reversal of isolation in ii turns. There should be 3 rings.
    TEST_DIFFERENCE(issue_869_a, 3, 3600, 0, 0, 3); // a went wrong

    TEST_DIFFERENCE(issue_888_34, 22, 0.2506824, 6, 0.0253798, 28); // a went wrong
    TEST_DIFFERENCE(issue_888_37, 15, 0.0451408, 65, 0.3014843, 80); // b went wrong

    {
        ut_settings settings;
        settings.validity_false_negative_a = true;
        settings.validity_false_negative_sym = true;
        TEST_DIFFERENCE_WITH(0, 1, issue_888_53, 117, 0.2973268, 17, 0.0525798, 134);
    }

    // Areas and #clips correspond with POSTGIS (except sym case)
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_101_multi",
        case_101_multi[0], case_101_multi[1],
            5, 23, 4.75,
            5, 40, 12.75,
            5, 48, 4.75 + 12.75);

    // Areas and #clips correspond with POSTGIS
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_102_multi",
        case_102_multi[0], case_102_multi[1],
            2, 8, 0.75,
            6, 25, 3.75,
            6, 27, 0.75 + 3.75);

    // Areas and #clips correspond with POSTGIS
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_107_multi",
        case_107_multi[0], case_107_multi[1],
            2, 11, 2.25,
            3, 14, 3.0,
            4, 21, 5.25);

    TEST_DIFFERENCE(case_133_multi, 3, 16.0, 2, 8.0, 5);
    TEST_DIFFERENCE(case_134_multi, 3, 16.0, 2, 8.0, 5);
    TEST_DIFFERENCE(case_135_multi, 2, 2.0, 2, 13.0, 2);
    TEST_DIFFERENCE(case_136_multi, 2, 2.0, 3, 13.5, 3);
    TEST_DIFFERENCE(case_137_multi, 2, 2.5, 2, 13.0, 2);
    TEST_DIFFERENCE(case_138_multi, 5, 16.6, 3, 8.225, 8);
    TEST_DIFFERENCE(case_139_multi, 4, 16.328125, 3, 8.078125, 7);
    TEST_DIFFERENCE(case_140_multi, 4, 16.328125, 3, 8.078125, 7);
    TEST_DIFFERENCE(case_141_multi, 5, 15.5, 5, 10.0, 10);

    // Areas correspond with POSTGIS,
    // #clips in PostGIS is 11,11,5 but should most probably be be 12,12,6
    TEST_DIFFERENCE(case_recursive_boxes_1, 12, 26.0, 12, 24.0, 6);

    // Areas and #clips correspond with POSTGIS
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_2",
        case_recursive_boxes_2[0], case_recursive_boxes_2[1],
            3, 15, 3.0,
            7, 33, 7.0,
            10, 48, 10.0);

    // Areas and #clips by POSTGIS (except sym case)
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_3",
        case_recursive_boxes_3[0], case_recursive_boxes_3[1],
            24, -1, 21.5,
            25, -1, 22.5,
            37, -1, 44.0);

    // 4, input is not valid

    TEST_DIFFERENCE(case_recursive_boxes_5, 16, 22.0, 12, 27.0, 10);
    TEST_DIFFERENCE(case_recursive_boxes_6, 7, 3.5, 3, 1.5, 9);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_7",
        case_recursive_boxes_7[0], case_recursive_boxes_7[1],
            3, 15, 2.75,
            4, 19, 2.75,
            3, 22, 5.5);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_8",
        case_recursive_boxes_8[0], case_recursive_boxes_8[1],
            2, -1, 2.50,
            4, -1, 5.75,
            4, -1, 8.25);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_9",
        case_recursive_boxes_9[0], case_recursive_boxes_9[1],
            3, -1, 1.5,
            4, -1, 2.5,
            6, -1, 4.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_10",
        case_recursive_boxes_10[0], case_recursive_boxes_10[1],
            2, -1, 1.25,
            2, -1, 0.75,
            4, -1, 2.00);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_11",
        case_recursive_boxes_11[0], case_recursive_boxes_11[1],
            3, -1, 2.5,
            3, -1, 4.5,
            3, -1, 7.0);

    TEST_DIFFERENCE(case_recursive_boxes_12, 4, 2.75, 3, 2.75, 6);
    TEST_DIFFERENCE(case_recursive_boxes_13, 4, 4.75, 3, 5.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_14, 3, 2.0, 4, 2.5, 5);
    TEST_DIFFERENCE(case_recursive_boxes_15, 3, 3.0, 2, 2.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_16, 8, 6.5, 3, 5.5, 9);
    TEST_DIFFERENCE(case_recursive_boxes_17, 10, 7.75, 7, 5.5, 13);
    TEST_DIFFERENCE(case_recursive_boxes_18, 2, 1.0, 1, 1.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_19, 2, 1.0, 2, 1.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_20, 2, 1.0, 0, 0.0, 2);

    TEST_DIFFERENCE(case_recursive_boxes_21, 2, 1.0, 1, 1.0, 1);
    TEST_DIFFERENCE(case_recursive_boxes_22, 2, 1.25, 2, 2.0, 2);
    TEST_DIFFERENCE(case_recursive_boxes_23, 2, 0.75, 1, 0.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_24, 3, 2.5, 2, 2.0, 5);
    TEST_DIFFERENCE(case_recursive_boxes_25, 2, 2.5, 3, 2.5, 2);
    TEST_DIFFERENCE(case_recursive_boxes_26, 2, 1.5, 3, 2.0, 4);
    TEST_DIFFERENCE(case_recursive_boxes_27, 1, 1.5, 3, 2.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_28, 3, 2.5, 2, 3.0, 4);
    TEST_DIFFERENCE(case_recursive_boxes_29, 5, 7.25, 5, 4.5, 5);
    TEST_DIFFERENCE(case_recursive_boxes_30, 6, 4.25, 3, 7.25, 7);

    TEST_DIFFERENCE(case_recursive_boxes_31, 2, 2.0, 1, 0.5, 2);
    TEST_DIFFERENCE(case_recursive_boxes_32, 2, 2.75, 2, 1.25, 2);
    TEST_DIFFERENCE(case_recursive_boxes_33, 4, 3.0, 3, 6.0, 4);
    TEST_DIFFERENCE(case_recursive_boxes_34, 7, 7.25, 1, 0.5, 8);
    TEST_DIFFERENCE(case_recursive_boxes_35, 5, 1.75, 5, 2.75, 10);
    TEST_DIFFERENCE(case_recursive_boxes_36, 2, 1.0, 2, 1.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_37, 3, 2.5, 2, 4.25, 2);
    TEST_DIFFERENCE(case_recursive_boxes_38, 5, 7.75, 4, 3.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_39, 3, 6.0, 3, 3.0, 4);
    TEST_DIFFERENCE(case_recursive_boxes_40, 11, 14.0, 9, 13.0, 11);

    TEST_DIFFERENCE(case_recursive_boxes_41, 1, 0.5, 1, 0.5, 2);
    TEST_DIFFERENCE(case_recursive_boxes_42, 1, 1.0, 4, 4.0, 5);
    TEST_DIFFERENCE(case_recursive_boxes_43, 1, 0.5, 3, 2.0, 4);
    TEST_DIFFERENCE(case_recursive_boxes_44, 3, 5.0, 0, 0.0, 3);
    TEST_DIFFERENCE(case_recursive_boxes_45, 6, 20.0, 7, 20.0, 3);
    TEST_DIFFERENCE(case_recursive_boxes_46, 4, 14.0, 5, 12.0, 5);
    TEST_DIFFERENCE(case_recursive_boxes_47, 4, 10.0, 7, 11.0, 1);
    TEST_DIFFERENCE(case_recursive_boxes_48, 0, 0.0, 1, 9.0, 1);
    TEST_DIFFERENCE(case_recursive_boxes_49, 10, 22.0, 10, 17.0, 11);
    TEST_DIFFERENCE(case_recursive_boxes_50, 14, 21.0, 16, 21.0, 14);
    TEST_DIFFERENCE(case_recursive_boxes_51, 14, 25.0, 12, 31.0, 7);
    TEST_DIFFERENCE(case_recursive_boxes_52, 13, 30.0, 15, 25.0, 8);
    TEST_DIFFERENCE(case_recursive_boxes_53, 6, 3.5, 4, 1.5, 9);
    TEST_DIFFERENCE(case_recursive_boxes_54, 6, 6.5, 8, 6.0, 7);
    TEST_DIFFERENCE(case_recursive_boxes_55, 4, 5.5, 6, 7.75, 4);
    TEST_DIFFERENCE(case_recursive_boxes_56, 4, 4.5, 5, 2.75, 6);
    TEST_DIFFERENCE(case_recursive_boxes_57, 5, 3.75, 9, 6.5, 10);
    TEST_DIFFERENCE(case_recursive_boxes_58, 4, 2.25, 6, 3.75, 7);
    TEST_DIFFERENCE(case_recursive_boxes_59, 8, 6.5, 7, 7.0, 12);
    TEST_DIFFERENCE(case_recursive_boxes_60, 6, 5.25, 7, 5.25, 11);
    TEST_DIFFERENCE(case_recursive_boxes_61, 2, 1.5, 6, 2.0, 7);
#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    // Misses one triangle, should be fixed in traversal.
    // It is not related to rescaling.
    TEST_DIFFERENCE(case_recursive_boxes_62, 5, 5.0, 11, 5.75, 12);
#endif

    TEST_DIFFERENCE(case_recursive_boxes_63, 9, 10.5, 5, 27.75, 4);
    TEST_DIFFERENCE(case_recursive_boxes_64, 6, 2.75, 7, 4.5, 11);
    TEST_DIFFERENCE(case_recursive_boxes_65, 6, 4.25, 7, 3.0, 13);
    TEST_DIFFERENCE(case_recursive_boxes_66, 5, 4.75, 7, 4.0, 9);
    TEST_DIFFERENCE(case_recursive_boxes_67, 7, 6.25, 9, 6.0, 10);
    TEST_DIFFERENCE(case_recursive_boxes_68, 10, 6.5, 9, 6.5, 7);
    TEST_DIFFERENCE(case_recursive_boxes_69, 5, 6.25, 5, 6.75, 8);
    TEST_DIFFERENCE(case_recursive_boxes_70, 5, 2.0, 8, 4.5, 11);
    TEST_DIFFERENCE(case_recursive_boxes_71, 7, 8.25, 7, 5.75, 8);
    TEST_DIFFERENCE(case_recursive_boxes_72, 6, 6.5, 7, 4.0, 10);
    TEST_DIFFERENCE(case_recursive_boxes_73, 4, 1.75, 5, 4.0, 8);
    TEST_DIFFERENCE(case_recursive_boxes_74, 3, 3.00, 3, 1.5, 5);
    TEST_DIFFERENCE(case_recursive_boxes_75, 7, 4.5, 4, 2.0, 11);
    TEST_DIFFERENCE(case_recursive_boxes_76, 7, 3.75, 4, 2.5, 9);
    TEST_DIFFERENCE(case_recursive_boxes_77, 4, 3.75, 7, 6.25, 8);
    TEST_DIFFERENCE(case_recursive_boxes_78, 11, 5.5, 8, 4.5, 14);
    TEST_DIFFERENCE(case_recursive_boxes_79, 2, 1.25, 6, 4.5, 8);

    // one polygon is divided into two, for same reason as union creates a small
    // interior ring there, which is acceptable
    TEST_DIFFERENCE(case_recursive_boxes_80, 1, 0.5, 2, 0.75, count_set(2, 3));

    TEST_DIFFERENCE(case_recursive_boxes_81, 3, 5.0, 6, 6.75, 6);
    TEST_DIFFERENCE(case_recursive_boxes_82, 5, 7.25, 7, 4.5, 8);
    TEST_DIFFERENCE(case_recursive_boxes_83, 9, 5.25, 8, 5.25, 12);
    TEST_DIFFERENCE(case_recursive_boxes_84, 4, 8.0, 7, 9.0, 4);
#if ! defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    TEST_DIFFERENCE(case_recursive_boxes_85, 4, 4.0, 7, 3.75, 9);
#endif

    TEST_DIFFERENCE(case_recursive_boxes_86, 1, 1.5, 2, 1.5, 3);
    TEST_DIFFERENCE(case_recursive_boxes_87, 4, 2.0, 4, 2.5, 8);
    TEST_DIFFERENCE(case_recursive_boxes_88, 3, 4.75, 5, 6.75, 4);

    // Output of A can be 0 or 1 polygons (with a very small area)
    TEST_DIFFERENCE(case_precision_m1, optional(), expectation_limits(0.0, 5.0e-7), 1, 57.0, count_set(1, 2));
    // Output of A can be 1 or 2 polygons (one with a very small area)
    TEST_DIFFERENCE(case_precision_m2, count_set(1, 2), 1.0, 1, 57.75, count_set(2, 3));

    {
        ut_settings settings;
        settings.sym_difference = BG_IF_RESCALED(true, BG_IF_TEST_FAILURES);
        test_one<Polygon, MultiPolygon, MultiPolygon>("mysql_21965285_b",
            mysql_21965285_b[0],
            mysql_21965285_b[1],
            2, -1, 183.71376870369406,
            2, -1, 131.21376870369406,
            settings);
    }

    TEST_DIFFERENCE(mysql_regression_1_65_2017_08_31,
                    optional(), optional_sliver(1e-6),
                    3, 152.064185, count_set(3, 4));
}


template <typename P>
void test_all()
{
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;
    test_areal<ring, polygon, multi_polygon>();
}


// Test cases for integer coordinates / ccw / open
template <typename Polygon, typename MultiPolygon>
void test_specific_areal()
{
    {
        // Spikes in a-b and b-a, failure in symmetric difference
        ut_settings settings;
        settings.sym_difference = false;
        settings.set_test_validity(false);

        TEST_DIFFERENCE_WITH(0, 1, ticket_11674,
                             count_set(3, 4),
                             expectation_limits(9105473, 9105782),
                             5,
                             expectation_limits(119059, 119424), ignore_count());
    }

    {
        // Ticket 12751 (Volker)
        // Spikes in a-b and b-a, failure in symmetric difference

        ut_settings settings;
        settings.remove_spikes = true;

        TEST_DIFFERENCE_WITH(0, 1, ticket_12751, 1,
                             expectation_limits(2781964, 2782115), 1,
                             expectation_limits(597.0, 598.0), 2);

        TEST_DIFFERENCE_WITH(2, 3, ticket_12751,
                             2, expectation_limits(2537992, 2538306),
                             2, expectation_limits(294736, 294964),
                             3);
    }

    {
        // Ticket 12752 (Volker)
        // Spikes in a-b and b-a, failure in symmetric difference
        ut_settings settings;
        settings.remove_spikes = true;
        settings.sym_difference = false;
        TEST_DIFFERENCE_WITH(0, 1, ticket_12752,
                             count_set(2, 3), expectation_limits(2776656, 2776693),
                             3, expectation_limits(7710, 7894),
                             2);
    }

    {
        const std::string a_min_b =
            test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_10661",
                          ticket_10661[0], ticket_10661[1],
                          2, -1, expectation_limits(1441632, 1441855),
                          2, -1, expectation_limits(13167454, 13182415),
                          count_set(3, 4));

        test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_10661_2",
            a_min_b, ticket_10661[2],
            1, 8, 825192.0,
            1, 10, expectation_limits(27226148, 27842812),
            count_set(1, 2));
    }

    {
        ut_settings settings;
        settings.sym_difference = false;

        TEST_DIFFERENCE_WITH(0, 1, ticket_9942, 4,
                             expectation_limits(7427727, 7428108), 4,
                             expectation_limits(130083, 131823), 4);
        TEST_DIFFERENCE_WITH(0, 1, ticket_9942a, 2,
                             expectation_limits(412676, 413469), 2,
                             expectation_limits(76779, 77038), 4);
    }
}

template <typename Point, bool ClockWise, bool Closed>
void test_specific()
{
    typedef bg::model::polygon<Point, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;
    test_specific_areal<polygon, multi_polygon>();
}


int test_main(int, char* [])
{
    BoostGeometryWriteTestConfiguration();
    test_all<bg::model::d2::point_xy<default_test_type> >();

    test_specific<bg::model::d2::point_xy<int>, false, false>();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<bg::model::d2::point_xy<float> >();
#endif

#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    // Not yet fully tested for float.
    // The difference algorithm can generate (additional) slivers
    BoostGeometryWriteExpectedFailures(24, 11, 21, 7);
#endif

    return 0;
}
