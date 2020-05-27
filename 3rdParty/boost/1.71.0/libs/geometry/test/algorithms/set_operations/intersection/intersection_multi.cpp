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

#include "test_intersection.hpp"
#include <algorithms/test_overlay.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/intersection.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/io/wkt/read.hpp>

#define TEST_INTERSECTION(caseid, clips, points, area) \
    (test_one<Polygon, MultiPolygon, MultiPolygon>) \
    ( #caseid, caseid[0], caseid[1], clips, points, area)

#define TEST_INTERSECTION_REV(caseid, clips, points, area) \
    (test_one<Polygon, MultiPolygon, MultiPolygon>) \
    ( #caseid "_rev", caseid[1], caseid[0], clips, points, area)

#define TEST_INTERSECTION_IGNORE(caseid, clips, points, area) \
    { ut_settings ignore_validity; ignore_validity.test_validity = false; \
    (test_one<Polygon, MultiPolygon, MultiPolygon>) \
    ( #caseid, caseid[0], caseid[1], clips, points, area, ignore_validity); }

template <typename Ring, typename Polygon, typename MultiPolygon>
void test_areal()
{
    test_one<Polygon, MultiPolygon, MultiPolygon>("simplex_multi",
        case_multi_simplex[0], case_multi_simplex[1],
        2, 12, 6.42);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_no_ip",
        case_multi_no_ip[0], case_multi_no_ip[1],
        2, 8, 8.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_2",
        case_multi_2[0], case_multi_2[1],
        3, 12, 5.9);

    test_one<Polygon, MultiPolygon, Polygon>("simplex_multi_mp_p",
        case_multi_simplex[0], case_single_simplex,
        2, 12, 6.42);

    test_one<Polygon, Ring, MultiPolygon>("simplex_multi_r_mp",
        case_single_simplex, case_multi_simplex[0],
        2, 12, 6.42);
    test_one<Ring, MultiPolygon, Polygon>("simplex_multi_mp_r",
        case_multi_simplex[0], case_single_simplex,
        2, 12, 6.42);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_a",
        case_58_multi[0], case_58_multi[3],
        3, 12, 0.666666667);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_b",
        case_58_multi[1], case_58_multi[2],
        1, 19, 11.16666666667);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_b4",
        case_58_multi[4], case_58_multi[2],
        1, 13, 12.66666666);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_b5",
        case_58_multi[5], case_58_multi[2],
        1, 13, 13.25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_b6",
        case_58_multi[6], case_58_multi[2],
        1, 13, 13.25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_58_multi_b7",
        case_58_multi[7], case_58_multi[2],
        1, 16, 12.5);

    // Constructed cases for multi/touch/equal/etc
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_61_multi",
        case_61_multi[0], case_61_multi[1],
        0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_62_multi",
        case_62_multi[0], case_62_multi[1],
        1, 5, 1.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_63_multi",
        case_63_multi[0], case_63_multi[1],
        1, 5, 1.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_64_multi",
        case_64_multi[0], case_64_multi[1],
        1, 5, 1.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_65_multi",
        case_65_multi[0], case_65_multi[1],
        1, 5, 1.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_65_multi_inv_a",
        case_65_multi[0], case_65_multi[3],
        0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_65_multi_inv_b",
        case_65_multi[1], case_65_multi[2],
        2, 10, 3.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_72_multi",
        case_72_multi[0], case_72_multi[1],
        3, 14, 2.85);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_72_multi_inv_b",
        case_72_multi[1], case_72_multi[2],
        3, 16, 6.15);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_77_multi",
        case_77_multi[0], case_77_multi[1],
        5, 27, 9.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_78_multi",
        case_78_multi[0], case_78_multi[1],
        1, 16, 22.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_101_multi",
        case_101_multi[0], case_101_multi[1],
        4, 22, 4.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_102_multi",
        case_102_multi[0], case_102_multi[1],
        3, 26, 19.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_102_multi_inv_b",
        case_102_multi[1], case_102_multi[2],
        6, 25, 3.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_107_multi",
        case_107_multi[0], case_107_multi[1],
        2, 10, 1.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_107_multi_inv_b",
        case_107_multi[1], case_107_multi[2],
        3, 13, 3.0);

#if ! defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    {
        ut_settings ignore_validity;

        // Rescaling misses one intersection
        test_one<Polygon, MultiPolygon, MultiPolygon>("case_108_multi",
            case_108_multi[0], case_108_multi[1],
            7, -1, 7.5,
            ignore_validity);
    }
#endif

    TEST_INTERSECTION(case_123_multi, 3, 13, 1.875);
    TEST_INTERSECTION(case_124_multi, 2, 13, 2.0625);
    TEST_INTERSECTION(case_125_multi, 3, 17, 2.1);
    TEST_INTERSECTION(case_126_multi, 5, 27, 9.0);
    TEST_INTERSECTION(case_127_multi, 3, 19, 24.0);
    TEST_INTERSECTION(case_128_multi, 2, 26, 75.5);
    TEST_INTERSECTION(case_129_multi, 1, 20, 20.5);
    TEST_INTERSECTION(case_130_multi, 2, 30, 39.0);

    TEST_INTERSECTION(case_133_multi, 2, 23, 40.625);
    TEST_INTERSECTION(case_134_multi, 1, 23, 42.0);
    TEST_INTERSECTION(case_135_multi, 1, 12, 7.0);
    TEST_INTERSECTION(case_136_multi, 1, 12, 6.5);
    TEST_INTERSECTION(case_137_multi, 1, 12, 6.5);

    TEST_INTERSECTION(case_138_multi, 2, 23, 40.4);
    TEST_INTERSECTION(case_139_multi, 2, 23, 40.546875);
    TEST_INTERSECTION(case_140_multi, 2, 23, 40.546875);
    TEST_INTERSECTION(case_141_multi, 3, -1, 74.5);

    TEST_INTERSECTION(case_recursive_boxes_1, 10, 89, 47.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_2",
        case_recursive_boxes_2[0], case_recursive_boxes_2[1],
        1, 50, 90.0); // Area from SQL Server
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_3",
        case_recursive_boxes_3[0], case_recursive_boxes_3[1],
        19, 84, 12.5); // Area from SQL Server

    TEST_INTERSECTION_IGNORE(case_recursive_boxes_4, 13, 158, 67.0);

    // Fixed by replacing handle_tangencies in less_by_segment_ratio sort order
    // Should contain 6 output polygons
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_6",
        case_recursive_boxes_6[0], case_recursive_boxes_6[1],
        6, 42, 19.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_7",
        case_recursive_boxes_7[0], case_recursive_boxes_7[1],
        2, 9, 1.5);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_8",
        case_recursive_boxes_8[0], case_recursive_boxes_8[1],
        3, 19, 3.75);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_9",
        case_recursive_boxes_9[0], case_recursive_boxes_9[1],
        5, 27, 4.25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_10",
        case_recursive_boxes_10[0], case_recursive_boxes_10[1],
        2, 8, 0.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_11",
        case_recursive_boxes_11[0], case_recursive_boxes_11[1],
        2, 8, 1.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_12",
        case_recursive_boxes_12[0], case_recursive_boxes_12[1],
        1, 4, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_13",
        case_recursive_boxes_13[0], case_recursive_boxes_13[1],
        0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_14",
        case_recursive_boxes_14[0], case_recursive_boxes_14[1],
        0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_15",
        case_recursive_boxes_15[0], case_recursive_boxes_15[1],
        1, 4, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_16",
        case_recursive_boxes_16[0], case_recursive_boxes_16[1],
        9, 43, 10.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_17",
        case_recursive_boxes_17[0], case_recursive_boxes_17[1],
        7, -1, 7.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_18",
        case_recursive_boxes_18[0], case_recursive_boxes_18[1],
        0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_19",
        case_recursive_boxes_19[0], case_recursive_boxes_19[1],
        0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_20",
        case_recursive_boxes_20[0], case_recursive_boxes_20[1],
        2, 0, 1.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_21",
        case_recursive_boxes_21[0], case_recursive_boxes_21[1],
        1, 0, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_22",
        case_recursive_boxes_22[0], case_recursive_boxes_22[1],
        0, 0, 0.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_23",
        case_recursive_boxes_23[0], case_recursive_boxes_23[1],
        1, 0, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_24",
        case_recursive_boxes_24[0], case_recursive_boxes_24[1],
        1, 0, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_25",
        case_recursive_boxes_25[0], case_recursive_boxes_25[1],
        1, 0, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_26",
        case_recursive_boxes_26[0], case_recursive_boxes_26[1],
        1, 0, 2.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_27",
        case_recursive_boxes_27[0], case_recursive_boxes_27[1],
        1, 0, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_28",
        case_recursive_boxes_28[0], case_recursive_boxes_28[1],
        2, 0, 1.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_29",
        case_recursive_boxes_29[0], case_recursive_boxes_29[1],
        5, 0, 3.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_30",
        case_recursive_boxes_30[0], case_recursive_boxes_30[1],
        4, 0, 6.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_31",
        case_recursive_boxes_31[0], case_recursive_boxes_31[1],
        2, 0, 2.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_32",
        case_recursive_boxes_32[0], case_recursive_boxes_32[1],
        2, 0, 1.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_33",
        case_recursive_boxes_33[0], case_recursive_boxes_33[1],
        3, 0, 2.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_34",
        case_recursive_boxes_34[0], case_recursive_boxes_34[1],
        2, 0, 17.25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_35",
        case_recursive_boxes_35[0], case_recursive_boxes_35[1],
        2, 0, 20.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_36",
        case_recursive_boxes_36[0], case_recursive_boxes_36[1],
        1, 0, 0.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_37",
        case_recursive_boxes_37[0], case_recursive_boxes_37[1],
        2, 0, 1.0);

    TEST_INTERSECTION(case_recursive_boxes_39, 3, 0, 3.0);
    TEST_INTERSECTION(case_recursive_boxes_40, 1, 0, 1.0);
    TEST_INTERSECTION(case_recursive_boxes_41, 1, 0, 23.5);
    TEST_INTERSECTION(case_recursive_boxes_42, 1, 29, 95.0);
    TEST_INTERSECTION(case_recursive_boxes_43, 2, 0, 22.5);
    TEST_INTERSECTION(case_recursive_boxes_44, 2, 0, 3.0);
    TEST_INTERSECTION(case_recursive_boxes_45, 7, 0, 12.0);
    TEST_INTERSECTION(case_recursive_boxes_46, 6, -1, 7.0);
    TEST_INTERSECTION(case_recursive_boxes_47, 1, 5, 1.0);
    TEST_INTERSECTION(case_recursive_boxes_48, 1, 5, 1.0);
    TEST_INTERSECTION(case_recursive_boxes_49, 7, 57, 20.0);
    TEST_INTERSECTION(case_recursive_boxes_50, 9, 62, 26.0);
    TEST_INTERSECTION(case_recursive_boxes_51, 14, 74, 19.0);
    TEST_INTERSECTION(case_recursive_boxes_52, 8, -1, 22.0);
    TEST_INTERSECTION(case_recursive_boxes_53, 1, -1, 19.75);
    TEST_INTERSECTION(case_recursive_boxes_54, 3, -1, 10.0);
    TEST_INTERSECTION(case_recursive_boxes_55, 5, -1, 2.25);
    TEST_INTERSECTION(case_recursive_boxes_56, 1, -1, 0.5);
    TEST_INTERSECTION(case_recursive_boxes_57, 10, -1, 9.5);
    TEST_INTERSECTION(case_recursive_boxes_58, 1, -1, 0.25);
    TEST_INTERSECTION(case_recursive_boxes_59, 8, -1, 8.25);
    TEST_INTERSECTION(case_recursive_boxes_60, 8, -1, 10.0);
    TEST_INTERSECTION(case_recursive_boxes_61, 2, -1, 20.0);
    TEST_INTERSECTION(case_recursive_boxes_62, 9, -1, 10.5);
    TEST_INTERSECTION(case_recursive_boxes_63, 11, -1, 5.75);
    TEST_INTERSECTION(case_recursive_boxes_64, 5, -1, 17.25);
    TEST_INTERSECTION(case_recursive_boxes_65, 3, -1, 17.25);
    TEST_INTERSECTION(case_recursive_boxes_66, 4, -1, 16.0);
    TEST_INTERSECTION(case_recursive_boxes_67, 5, -1, 2.5);
    TEST_INTERSECTION(case_recursive_boxes_68, 8, -1, 9.5);
    TEST_INTERSECTION(case_recursive_boxes_69, 6, -1, 3.25);
    TEST_INTERSECTION(case_recursive_boxes_70, 6, -1, 18.5);
    TEST_INTERSECTION(case_recursive_boxes_71, 3, -1, 1.75);
    TEST_INTERSECTION(case_recursive_boxes_72, 8, -1, 4.5);
    TEST_INTERSECTION(case_recursive_boxes_73, 3, -1, 18.5);
    TEST_INTERSECTION(case_recursive_boxes_74, 3, -1, 20.25);
    TEST_INTERSECTION(case_recursive_boxes_75, 5, -1, 16.75);
    TEST_INTERSECTION(case_recursive_boxes_76, 2, -1, 18.25);
    TEST_INTERSECTION(case_recursive_boxes_77, 5, -1, 3.5);
    TEST_INTERSECTION(case_recursive_boxes_78, 9, -1, 8.0);
    TEST_INTERSECTION(case_recursive_boxes_79, 5, -1, 9.0);
    TEST_INTERSECTION(case_recursive_boxes_80, 1, -1, 0.25);
    TEST_INTERSECTION(case_recursive_boxes_81, 5, -1, 3.75);
    TEST_INTERSECTION(case_recursive_boxes_82, 5, -1, 8.5);
    TEST_INTERSECTION(case_recursive_boxes_83, 5, -1, 10.25);
    TEST_INTERSECTION(case_recursive_boxes_84, 1, -1, 0.5);
#if ! defined(BOOST_GEOMETRY_USE_RESCALING)
    TEST_INTERSECTION(case_recursive_boxes_85, 1, -1, 0.25);
#endif
    TEST_INTERSECTION(case_recursive_boxes_86, 0, -1, 0.0);
    TEST_INTERSECTION(case_recursive_boxes_87, 0, -1, 0.0);
    TEST_INTERSECTION(case_recursive_boxes_88, 4, -1, 3.5);

#if ! defined(BOOST_GEOMETRY_USE_RESCALING)
    TEST_INTERSECTION(case_precision_m1, 1, -1, 14.0);
    TEST_INTERSECTION(case_precision_m2, 2, -1, 15.25);
    TEST_INTERSECTION_REV(case_precision_m1, 1, -1, 14.0);
    TEST_INTERSECTION_REV(case_precision_m2, 2, -1, 15.25);
#else
    // Validity: false positives (very small triangles looking like a line)
    TEST_INTERSECTION_IGNORE(case_precision_m1, 1, -1, 14.0);
    TEST_INTERSECTION_IGNORE(case_precision_m2, 2, -1, 15.25);
#endif

    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_a",
        ggl_list_20120915_h2[0], ggl_list_20120915_h2[1],
        2, 10, 6.0); // Area from SQL Server
    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_b",
        ggl_list_20120915_h2[0], ggl_list_20120915_h2[2],
        2, 10, 6.0); // Area from SQL Server

    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_9081",
        ticket_9081[0], ticket_9081[1],
        2, 10, 0.0019812556);

    // Should generate output, even for <float>
    test_one<Polygon, MultiPolygon, MultiPolygon>("mail_2019_01_21_johan",
        mail_2019_01_21_johan[2], mail_2019_01_21_johan[3],
        2, -1, 0.0005889587);

    // Very small slice is generated.
    // qcc-arm reports 1.7791215549400884e-14
    // With rescaling, generates very small triangle
    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_11018",
        ticket_11018[0], ticket_11018[1],
        BG_IF_RESCALED(1, 0), 0,
        1.0e-8, ut_settings(-1)
    );

    TEST_INTERSECTION(ticket_12503, 2, 13, 17.375);

    test_one<Polygon, MultiPolygon, MultiPolygon>("mysql_23023665_7",
        mysql_23023665_7[0], mysql_23023665_7[1],
        2, 11, 9.80505786783);

    TEST_INTERSECTION(mysql_23023665_12, 2, 0, 11.812440191387557);
    TEST_INTERSECTION(mysql_regression_1_65_2017_08_31, 2, -1, 29.9022122);
}

template <typename Polygon, typename MultiPolygon, typename Box>
void test_areal_clip()
{
    static std::string const clip = "POLYGON((1 1,4 4))";
    test_one<Polygon, Box, MultiPolygon>("simplex_multi_mp_b", clip, case_multi_simplex[0],
        2, 11, 6.791666);
    test_one<Polygon, MultiPolygon, Box>("simplex_multi_b_mp", case_multi_simplex[0], clip,
        2, 11, 6.791666);
}

template <typename LineString, typename MultiLineString, typename Box>
void test_linear()
{
    typedef typename bg::point_type<MultiLineString>::type point;
    test_one<point, MultiLineString, MultiLineString>("case_multi_ml_ml_1",
        "MULTILINESTRING((0 0,1 1))", "MULTILINESTRING((0 1,1 0))",
        1, 1, 0.0);
    test_one<point, MultiLineString, MultiLineString>("case_multi_ml_ml_2",
        "MULTILINESTRING((0 0,1 1),(0.5 0,1.5 1))", "MULTILINESTRING((0 1,1 0),(0.5 1,1.5 0))",
        4, 4, 0.0);

    test_one<point, LineString, MultiLineString>("case_multi_l_ml",
        "LINESTRING(0 0,1 1)", "MULTILINESTRING((0 1,1 0),(0.5 1,1.5 0))",
        2, 2, 0.0);
    test_one<point, MultiLineString, LineString>("case_multi_ml_l",
        "MULTILINESTRING((0 1,1 0),(0.5 1,1.5 0))", "LINESTRING(0 0,1 1)",
        2, 2, 0.0);

    test_one<LineString, MultiLineString, Box>("case_multi_ml_b",
        "MULTILINESTRING((0 0,3 3)(1 0,4 3))", "POLYGON((1 1,3 2))",
        2, 4, 2.0 * std::sqrt(2.0));
    test_one<LineString, Box, MultiLineString>("case_multi_b_ml",
        "POLYGON((1 1,3 2))", "MULTILINESTRING((0 0,3 3)(1 0,4 3))",
        2, 4, 2.0 * std::sqrt(2.0));
}

template <typename P>
void test_point_output()
{
    typedef bg::model::box<P> box;
    typedef bg::model::linestring<P> linestring;
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    test_point_output<multi_polygon, multi_polygon>(case_multi_simplex[0], case_multi_simplex[1], 10);
    test_point_output<linestring, multi_polygon>("linestring(4 0,0 4)", case_multi_simplex[0], 4);
    test_point_output<box, multi_polygon>("box(3 0,4 6)", case_multi_simplex[0], 8);
}

template <typename MultiPolygon, typename MultiLineString>
void test_areal_linear()
{
    typedef typename boost::range_value<MultiPolygon>::type Polygon;
    typedef typename boost::range_value<MultiLineString>::type LineString;
    typedef typename bg::point_type<Polygon>::type Point;
    typedef bg::model::ring<Point> Ring;

    test_one_lp<LineString, MultiPolygon, LineString>("case_mp_ls_1", case_multi_simplex[0], "LINESTRING(2 0,2 5)", 2, 4, 3.70);
    test_one_lp<LineString, Polygon, MultiLineString>("case_p_mls_1", case_single_simplex, "MULTILINESTRING((2 0,2 5),(3 0,3 5))", 2, 4, 7.5);
    test_one_lp<LineString, MultiPolygon, MultiLineString>("case_mp_mls_1", case_multi_simplex[0], "MULTILINESTRING((2 0,2 5),(3 0,3 5))", 4, 8, 6.8333333);
    test_one_lp<LineString, Ring, MultiLineString>("case_r_mls_1", case_single_simplex, "MULTILINESTRING((2 0,2 5),(3 0,3 5))", 2, 4, 7.5);
}

template <typename P>
void test_all()
{
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;
    test_areal<ring, polygon, multi_polygon>();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)

    typedef bg::model::ring<P, false> ring_ccw;
    typedef bg::model::polygon<P, false> polygon_ccw;
    typedef bg::model::multi_polygon<polygon_ccw> multi_polygon_ccw;
    test_areal<ring_ccw, polygon_ccw, multi_polygon_ccw>();

    typedef bg::model::ring<P, true, false> ring_open;
    typedef bg::model::polygon<P, true, false> polygon_open;
    typedef bg::model::multi_polygon<polygon_open> multi_polygon_open;
    test_areal<ring_open, polygon_open, multi_polygon_open>();

    typedef bg::model::ring<P, false, false> ring_open_ccw;
    typedef bg::model::polygon<P, false, false> polygon_open_ccw;
    typedef bg::model::multi_polygon<polygon_open_ccw> multi_polygon_open_ccw;
    test_areal<ring_open_ccw, polygon_open_ccw, multi_polygon_open_ccw>();

    typedef bg::model::box<P> box;
    test_areal_clip<polygon, multi_polygon, box>();
    test_areal_clip<polygon_ccw, multi_polygon_ccw, box>();

    typedef bg::model::linestring<P> linestring;
    typedef bg::model::multi_linestring<linestring> multi_linestring;

    test_linear<linestring, multi_linestring, box>();
    test_areal_linear<multi_polygon, multi_linestring>();
#endif

    test_point_output<P>();
    // linear

}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double> >();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<bg::model::d2::point_xy<float> >();

#if defined(HAVE_TTMATH)
    std::cout << "Testing TTMATH" << std::endl;
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

#endif

    return 0;
}
