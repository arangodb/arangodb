// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2016.
// Modifications copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

#include "test_union.hpp"
#include <algorithms/test_overlay.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/within.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/io/wkt/read.hpp>


#define TEST_UNION(caseid, clips, holes, points, area) \
    (test_one<Polygon, MultiPolygon, MultiPolygon>) \
    ( #caseid, caseid[0], caseid[1], clips, holes, points, area)

#define TEST_UNION_IGNORE(caseid, clips, holes, points, area) \
   { ut_settings ignore_validity; ignore_validity.test_validity = false; \
     test_one<Polygon, MultiPolygon, MultiPolygon> \
     (#caseid, caseid[0], caseid[1], \
     clips, holes, points, area, ignore_validity); }


template <typename Ring, typename Polygon, typename MultiPolygon>
void test_areal()
{
    test_one<Polygon, MultiPolygon, MultiPolygon>("simplex_multi",
        case_multi_simplex[0], case_multi_simplex[1],
        1, 0, 20, 14.58);

    test_one<Polygon, Polygon, MultiPolygon>("simplex_multi_p_mp",
        case_single_simplex, case_multi_simplex[0],
        1, 0, 20, 14.58);
    test_one<Polygon, MultiPolygon, Polygon>("simplex_multi_mp_p",
        case_multi_simplex[0], case_single_simplex,
        1, 0, 20, 14.58);

    test_one<Polygon, Ring, MultiPolygon>("simplex_multi_r_mp",
        case_single_simplex, case_multi_simplex[0],
        1, 0, 20, 14.58);
    test_one<Ring, MultiPolygon, Polygon>("simplex_multi_mp_r",
        case_multi_simplex[0], case_single_simplex,
        1, 0, 20, 14.58);


    // Normal test cases
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_no_ip",
        case_multi_no_ip[0], case_multi_no_ip[1],
        4, 0, 16, 66.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_2",
        case_multi_2[0], case_multi_2[1],
        3, 0, 16, 59.1);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_a",
        case_58_multi[0], case_58_multi[3],
        2, 0, 21, 19.83333333);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_b",
        case_58_multi[1], case_58_multi[2],
        1, 3, 17, 48.333333);

    // Constructed cases for multi/touch/equal/etc
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_61_multi",
        case_61_multi[0], case_61_multi[1],
        1, 0, 11, 4.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_62_multi",
        case_62_multi[0], case_62_multi[1],
        2, 0, 10, 2.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_63_multi",
        case_63_multi[0], case_63_multi[1],
        2, 0, 10, 2.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_64_multi",
        case_64_multi[0], case_64_multi[1],
        1, 0, 9, 3.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_65_multi",
        case_65_multi[0], case_65_multi[1],
        3, 0, 15, 4.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_66_multi",
        case_66_multi[0], case_66_multi[1],
        3, 0, 23, 7.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_72_multi",
        case_72_multi[0], case_72_multi[1],
        1, 0, 13, 10.65);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_75_multi",
        case_75_multi[0], case_75_multi[1],
        5, 0, 25, 5.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_76_multi",
        case_76_multi[0], case_76_multi[1],
        5, 0, 31, 8.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_89_multi",
        case_89_multi[0], case_89_multi[1],
        1, 0, 13, 6);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_101_multi",
        case_101_multi[0], case_101_multi[1],
        1, 3, 32, 22.25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_103_multi",
        case_103_multi[0], case_103_multi[1],
        1, 0, 7, 25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_104_multi",
        case_104_multi[0], case_104_multi[1],
        1, 0, 8, 25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_105_multi",
        case_105_multi[0], case_105_multi[1],
        1, 0, 5, 25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_106_multi",
        case_106_multi[0], case_106_multi[1],
        1, 0, 5, 25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_107_multi",
        case_107_multi[0], case_107_multi[1],
        1, 0, 15, 6.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_108_multi",
        case_108_multi[0], case_108_multi[1],
        1, 1, 20, 22.75);

    TEST_UNION(case_109_multi, 1, 2, 14, 1400);

    // Should have 9 holes, they are all separate and touching
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_110_multi",
       case_110_multi[0], case_110_multi[1],
       1, 9, 45, 1250);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_111_multi",
       case_111_multi[0], case_111_multi[1],
       2, 0, 10, 16);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_112_multi",
       case_112_multi[0], case_112_multi[1],
       2, 0, 16, 48);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_113_multi",
       case_113_multi[0], case_113_multi[1],
       2, 0, 13, 162.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_114_multi",
       case_114_multi[0], case_114_multi[1],
       1, 1, 13, 187.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_115_multi",
       case_115_multi[0], case_115_multi[1],
       1, 1, 18, 26.7036);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_116_multi",
       case_116_multi[0], case_116_multi[1],
       1, 2, 27, 51);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_117_multi",
       case_117_multi[0], case_117_multi[1],
       2, 0, 18, 22);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_118_multi",
       case_118_multi[0], case_118_multi[1],
       3, 0, 27, 46);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_119_multi",
       case_119_multi[0], case_119_multi[1],
       2, 0, 26, 44);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_120_multi",
       case_120_multi[0], case_120_multi[1],
       1, 1, 17, 35);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_121_multi",
       case_121_multi[0], case_121_multi[1],
       1, 1, 14, 25.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_122_multi",
       case_122_multi[0], case_122_multi[1],
       1, 1, 14, 29.5);

    TEST_UNION(case_123_multi, 1, 0, 11, 2.75);
    TEST_UNION(case_124_multi, 1, 0, 9, 2.75);
    TEST_UNION(case_125_multi, 1, 0, 9, 2.75);
    TEST_UNION(case_126_multi, 1, 2, 27, 52.0);

    TEST_UNION(case_131_multi, 1, 2, 15, 14.0);

    // SQL Server returns: MULTIPOLYGON (((4 4, 5.5 4.5, 6 6, 4.5 5.5, 4 4)), ((2 2, 3.5 2.5, 4 4, 2.5 3.5, 2 2)), ((0 0, 8 0, 8 8, 0 8, 0 0), (2 2, 2 4, 4 4, 4 6, 6 6, 6 4, 4 4, 4 2, 2 2)))
    // Which is one self-connected hole with two island polygons in both parts, basically identical to what Boost.Geometry delivers

    // PostGIS returns: MULTIPOLYGON(((0 0,0 8,8 8,8 0,0 0),(4 6,4 4,6 4,6 6,4 6)),((2 2,2.5 3.5,4 4,3.5 2.5,2 2),(4 4,2 4,2 2,4 2,4 4)),((4 4,4.5 5.5,6 6,5.5 4.5,4 4)))
    // Which seems wrong because the second hole is part of a smaller polygon (?)
    // ("POSTGIS="2.1.7 r13414" GEOS="3.5.0dev-CAPI-1.9.0 r4057")
    TEST_UNION(case_132_multi, 3, 2, 26, 60.0);

    TEST_UNION(case_133_multi, 2, 1, -1, 64.625);
    TEST_UNION(case_134_multi, 1, 2, -1, 66.0);
    TEST_UNION(case_135_multi, 1, 2, -1, 22.0);
    TEST_UNION(case_136_multi, 1, 2, -1, 22.0);
    TEST_UNION(case_137_multi, 1, 2, -1, 22.0);
    TEST_UNION(case_138_multi, 2, 1, -1, 65.225);
    TEST_UNION(case_139_multi, 2, 1, -1, 64.953);
    TEST_UNION(case_140_multi, 2, 1, -1, 64.953);
    TEST_UNION(case_141_multi, 1, 0, -1, 100.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_1",
        case_recursive_boxes_1[0], case_recursive_boxes_1[1],
        1, 1, 16, 97.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_2",
        case_recursive_boxes_2[0], case_recursive_boxes_2[1],
        1, 0, 5, 100.0); // Area from SQL Server
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_3",
        case_recursive_boxes_3[0], case_recursive_boxes_3[1],
        17, 6, 154, 56.5); // Area from SQL Server

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_4",
        case_recursive_boxes_4[0], case_recursive_boxes_4[1],
        1, 2, 26, 96.75);

    TEST_UNION(case_recursive_boxes_5, 3, 10, 98, 70.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_6",
        case_recursive_boxes_6[0], case_recursive_boxes_6[1],
        1, 3, 17, 24.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_7",
        case_recursive_boxes_7[0], case_recursive_boxes_7[1],
        2, 0, 20, 7.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_8",
        case_recursive_boxes_8[0], case_recursive_boxes_8[1],
        1, 0, 13, 12.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_9",
        case_recursive_boxes_9[0], case_recursive_boxes_9[1],
        1, 1, 11, 8.25);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_10",
        case_recursive_boxes_10[0], case_recursive_boxes_10[1],
            1, 0, -1, 2.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_11",
        case_recursive_boxes_11[0], case_recursive_boxes_11[1],
            1, 0, -1, 8.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_12",
        case_recursive_boxes_12[0], case_recursive_boxes_12[1],
            6, 0, -1, 6.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_13",
        case_recursive_boxes_13[0], case_recursive_boxes_13[1],
            3, 0, -1, 10.25);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_14",
        case_recursive_boxes_14[0], case_recursive_boxes_14[1],
            5, 0, -1, 4.5);

    // 12, 13, 14 with invalid input. To make then valid it is necessary
    // to break regions at self-intersection points (postponed)

    TEST_UNION_IGNORE(case_recursive_boxes_12_invalid, 5, 0, -1, 6.0);
    TEST_UNION_IGNORE(case_recursive_boxes_13_invalid, 2, 0, -1, 10.25);
    TEST_UNION_IGNORE(case_recursive_boxes_14_invalid, 4, 0, -1, 4.5);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_15",
        case_recursive_boxes_15[0], case_recursive_boxes_15[1],
            3, 0, -1, 6.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_16",
        case_recursive_boxes_16[0], case_recursive_boxes_16[1],
            1, 4, -1, 22.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_17",
        case_recursive_boxes_17[0], case_recursive_boxes_17[1],
            5, 2, -1, 21.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_18",
        case_recursive_boxes_18[0], case_recursive_boxes_18[1],
            3, 0, -1, 2.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_19",
        case_recursive_boxes_19[0], case_recursive_boxes_19[1],
            3, 0, -1, 2.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_20",
        case_recursive_boxes_20[0], case_recursive_boxes_20[1],
            2, 0, -1, 2.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_21",
        case_recursive_boxes_21[0], case_recursive_boxes_21[1],
            1, 0, -1, 2.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_22",
        case_recursive_boxes_22[0], case_recursive_boxes_22[1],
            2, 0, -1, 3.25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_23",
        case_recursive_boxes_23[0], case_recursive_boxes_23[1],
            3, 0, -1, 1.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_24",
        case_recursive_boxes_24[0], case_recursive_boxes_24[1],
            5, 0, -1, 5.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_25",
        case_recursive_boxes_25[0], case_recursive_boxes_25[1],
            2, 0, -1, 5.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_26",
        case_recursive_boxes_26[0], case_recursive_boxes_26[1],
            3, 0, -1, 6.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_27",
        case_recursive_boxes_27[0], case_recursive_boxes_27[1],
            4, 0, -1, 4.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_28",
        case_recursive_boxes_28[0], case_recursive_boxes_28[1],
            2, 0, -1, 6.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_29",
        case_recursive_boxes_29[0], case_recursive_boxes_29[1],
            2, 2, -1, 15.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_30",
        case_recursive_boxes_30[0], case_recursive_boxes_30[1],
            1, 3, -1, 17.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_31",
        case_recursive_boxes_31[0], case_recursive_boxes_31[1],
            3, 0, -1, 5.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_32",
        case_recursive_boxes_32[0], case_recursive_boxes_32[1],
            2, 0, -1, 5.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_33",
        case_recursive_boxes_33[0], case_recursive_boxes_33[1],
            1, 1, -1, 11.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_34",
        case_recursive_boxes_34[0], case_recursive_boxes_34[1],
            1, 0, -1, 25.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_35",
        case_recursive_boxes_35[0], case_recursive_boxes_35[1],
            1, 1, -1, 24.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_36",
        case_recursive_boxes_36[0], case_recursive_boxes_36[1],
            3, 0, -1, 3.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_37",
        case_recursive_boxes_37[0], case_recursive_boxes_37[1],
            2, 1, -1, 7.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_38",
        case_recursive_boxes_38[0], case_recursive_boxes_38[1],
            2, 1, -1, 14.0);

    TEST_UNION(case_recursive_boxes_46, 1, 4, 51, 33.0);
    TEST_UNION(case_recursive_boxes_47, 1, 0, -1, 22.0);
    TEST_UNION(case_recursive_boxes_48, 1, 1, -1, 10.0);
    TEST_UNION(case_recursive_boxes_49, 1, 3, -1, 59.0);
    TEST_UNION(case_recursive_boxes_50, 7, 4, -1, 68.0);
    TEST_UNION(case_recursive_boxes_51, 2, 6, -1, 75.0);
    TEST_UNION(case_recursive_boxes_52, 2, 6, -1, 77.0);
    TEST_UNION(case_recursive_boxes_53, 1, 1, -1, 24.75);
    TEST_UNION(case_recursive_boxes_54, 1, 2, -1, 22.5);
    TEST_UNION(case_recursive_boxes_55, 3, 1, -1, 15.5);
    TEST_UNION(case_recursive_boxes_56, 5, 1, -1, 7.75);
    TEST_UNION(case_recursive_boxes_57, 3, 4, -1, 19.75);
    TEST_UNION(case_recursive_boxes_58, 6, 1, -1, 6.25);
    TEST_UNION(case_recursive_boxes_59, 1, 3, -1, 21.75);
    TEST_UNION(case_recursive_boxes_60, 3, 0, -1, 20.5);
    TEST_UNION(case_recursive_boxes_61, 1, 1, -1, 23.5);
    TEST_UNION(case_recursive_boxes_62, 2, 3, -1, 21.25);
    TEST_UNION(case_recursive_boxes_63, 2, 3, -1, 44.0);
    TEST_UNION(case_recursive_boxes_64, 1, 2, -1, 24.5);
    TEST_UNION(case_recursive_boxes_65, 1, 1, -1, 24.5);
    TEST_UNION(case_recursive_boxes_66, 1, 1, -1, 24.75);
    TEST_UNION(case_recursive_boxes_67, 4, 0, -1, 14.75);
    TEST_UNION(case_recursive_boxes_68, 1, 4, -1, 22.5);
    TEST_UNION(case_recursive_boxes_69, 4, 0, -1, 16.25);
    TEST_UNION(case_recursive_boxes_70, 1, 0, -1, 25.0);
    TEST_UNION(case_recursive_boxes_71, 4, 2, -1, 15.75);
    TEST_UNION(case_recursive_boxes_72, 10, 0, -1, 15.0);
    TEST_UNION(case_recursive_boxes_73, 1, 2, -1, 24.25);
    TEST_UNION(case_recursive_boxes_74, 1, 1, -1, 24.75);
    TEST_UNION(case_recursive_boxes_75, 1, 2, -1, 23.25);
    TEST_UNION(case_recursive_boxes_76, 1, 0, -1, 24.5);
    TEST_UNION(case_recursive_boxes_77, 8, 1, -1, 13.5);
    TEST_UNION(case_recursive_boxes_78, 2, 5, -1, 18.0);
    TEST_UNION(case_recursive_boxes_79, 1, 2, -1, 14.75);

    // No hole should be generated (but rescaling generates one hole)
    TEST_UNION(case_recursive_boxes_80, 2, BG_IF_RESCALED(1, 0), -1, 1.5);

    TEST_UNION(case_recursive_boxes_81, 5, 0, -1, 15.5);
    TEST_UNION(case_recursive_boxes_82, 2, 2, -1, 20.25);
    TEST_UNION(case_recursive_boxes_83, 3, 1, -1, 20.75);
    TEST_UNION(case_recursive_boxes_84, 4, 1, -1, 17.5);
    TEST_UNION(case_recursive_boxes_85, 9, 0, -1, 8.0);
    TEST_UNION(case_recursive_boxes_86, 3, 0, -1, 3.0);
    TEST_UNION(case_recursive_boxes_87, 8, 0, -1, 4.5);
    TEST_UNION(case_recursive_boxes_88, 5, 1, -1, 15.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_a",
         ggl_list_20120915_h2[0], ggl_list_20120915_h2[1],
         1, 0, 12, 23.0); // Area from SQL Server
    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_b",
         ggl_list_20120915_h2[0], ggl_list_20120915_h2[2],
         1, 0, 12, 23.0); // Area from SQL Server

    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20140212_sybren",
         ggl_list_20140212_sybren[0], ggl_list_20140212_sybren[1],
         2, 0, 16, 0.002471626);

    {
        // Generates either 4 or 3 output polygons
        // With rescaling the result is invalid.
        ut_settings settings;
        settings.test_validity = BG_IF_RESCALED(false, true);
        test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_9081",
            ticket_9081[0], ticket_9081[1],
            BG_IF_RESCALED(4, 3), 0, 31, 0.2187385,
            settings);
    }

    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_10803",
        ticket_10803[0], ticket_10803[1],
        1, 0, 9, 2663736.07038);
    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_11984",
        ticket_11984[0], ticket_11984[1],
        1, 2, 134, 60071.08077);
    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_12118",
        ticket_12118[0], ticket_12118[1],
        1, -1, 27, 2221.38713);

#if defined(BOOST_GEOMETRY_TEST_FAILURES) || ! defined(BOOST_GEOMETRY_USE_RESCALING)
    // No output if rescaling is done
    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_12125",
        ticket_12125[0], ticket_12125[1],
        1, 0, -1, 575.831180350007);
#endif

    TEST_UNION(ticket_12503, 42, 1, -1, 945.625);

#if defined(BOOST_GEOMETRY_USE_KRAMER_RULE)
    // Two polygons, should ideally be merged
    TEST_UNION(mail_2019_01_21_johan, 2, 0, -1, 0.00058896);
#else
    // Correct: one polygon
    TEST_UNION(mail_2019_01_21_johan, 1, 0, -1, 0.00058896);
#endif

    TEST_UNION(mysql_23023665_7, 1, 1, -1, 99.19494);
    TEST_UNION(mysql_23023665_8, 1, 2, -1, 1400.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("mysql_23023665_9",
        mysql_23023665_9[0], mysql_23023665_9[1],
        1, 9, -1, 1250.0);

    TEST_UNION(mysql_regression_1_65_2017_08_31, 3, 0, -1, 181.966397646608);
}

// Test cases (generic)
template <typename Point, bool ClockWise, bool Closed>
void test_all()
{

    typedef bg::model::ring<Point, ClockWise, Closed> ring;
    typedef bg::model::polygon<Point, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;
    test_areal<ring, polygon, multi_polygon>();
}

// Test cases for integer coordinates / ccw / open
template <typename Point, bool ClockWise, bool Closed>
void test_specific()
{
    typedef bg::model::polygon<Point, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    ut_settings settings;
    settings.test_validity = true;

    test_one<polygon, multi_polygon, multi_polygon>("ticket_10803",
        ticket_10803[0], ticket_10803[1],
        1, 0, 9, 2664270, settings);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double>, true, true>();
#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<bg::model::d2::point_xy<double>, false, false>();

    test_specific<bg::model::d2::point_xy<int>, false, false>();

    test_all<bg::model::d2::point_xy<float>, true, true>();

#if defined(HAVE_TTMATH)
    std::cout << "Testing TTMATH" << std::endl;
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

#endif

    return 0;
}
