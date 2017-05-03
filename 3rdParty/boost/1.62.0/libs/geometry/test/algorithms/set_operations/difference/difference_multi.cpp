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

template <typename Ring, typename Polygon, typename MultiPolygon>
void test_areal()
{
    ut_settings ignore_validity;
    ignore_validity.test_validity = false;

    ut_settings sym_settings;
#if defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    sym_settings.sym_difference = false;
#endif

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
            3, 13, 1.65, 3, 17, 6.15, ignore_validity);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_77_multi",
        case_77_multi[0], case_77_multi[1],
            6, 31, 7.0,
            5, 36, 13.0,
            5, 43, 7.0 + 13.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_78_multi",
        case_78_multi[0], case_78_multi[1],
            1, 5, 1.0, 1, 5, 1.0);

    {
        ut_settings settings;

#if !defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
        settings.sym_difference = false;
#endif

        test_one<Polygon, MultiPolygon, MultiPolygon>("case_108_multi",
            case_108_multi[0], case_108_multi[1],
                7, 32, 5.5,
                4, 28, 9.75,
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
        settings.test_validity = false;

        test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120221_volker",
            ggl_list_20120221_volker[0], ggl_list_20120221_volker[1],
                2, 12, 7962.66, 1, 18, 2775258.93,
                settings);
    }

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_9081",
        ticket_9081[0], ticket_9081[1],
            2, 28, 0.0907392476356186, 4, 25, 0.126018011439877,
            4, 42, 0.0907392476356186 + 0.126018011439877,
            tolerance(0.001));

    // POSTGIS areas: 3.75893745345145, 2.5810000723917e-15
    test_one<Polygon, MultiPolygon, MultiPolygon>("bug_21155501",
        bug_21155501[0], bug_21155501[1],
            1, 9, 3.758937,
            0, 0, 0.0,
            ignore_validity);
#endif

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

    // Areas correspond with POSTGIS,
    // #clips in PostGIS is 11,11,5 but should most probably be be 12,12,6
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_1",
        case_recursive_boxes_1[0], case_recursive_boxes_1[1],
            11, 75, 26.0,
            12, 77, 24.0,
             5, 98, 50.0,
            ignore_validity);

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

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_5",
        case_recursive_boxes_5[0], case_recursive_boxes_5[1],
            15, -1, 22.0, // #clips should be 16
            11, -1, 27.0, // #clips should be 12
             8, -1, 49.0,
            ignore_validity);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_6",
        case_recursive_boxes_6[0], case_recursive_boxes_6[1],
            6, -1, 3.5,
            3, -1, 1.5,
            8, -1, 5.0,
            ignore_validity);

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

    test_one<Polygon, MultiPolygon, MultiPolygon>("mysql_21965285_b",
        mysql_21965285_b[0],
        mysql_21965285_b[1],
        2, -1, 183.71376870369406,
        2, -1, 131.21376870369406,
        sym_settings);
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
template <typename Point, bool ClockWise, bool Closed>
void test_specific()
{
    typedef bg::model::polygon<Point, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    {
        // Spikes in a-b and b-a, failure in symmetric difference

        ut_settings settings;
        settings.sym_difference = false;
        settings.test_validity = false;
#ifdef BOOST_GEOMETRY_TEST_INCLUDE_FAILING_TESTS
        settings.test_validity = true;
        settings.sym_difference = true;
#endif

        test_one<polygon, multi_polygon, multi_polygon>("ticket_11674",
            ticket_11674[0], ticket_11674[1],
            3, 27, 9105781.5,
            5, 22, 119059.5,
            2, -1, -1,
            settings);
    }

    {
        ut_settings settings;
        settings.test_validity = true;

        std::string a_min_b =
            test_one<polygon, multi_polygon, multi_polygon>("ticket_10661_1",
                ticket_10661[0], ticket_10661[1],
                2, 11, 1441632.5,
                2, 7, 13167454,
                settings);

        settings.test_validity = false;
#ifdef BOOST_GEOMETRY_TEST_INCLUDE_FAILING_TESTS
        settings.test_validity = true;
#endif
        test_one<polygon, multi_polygon, multi_polygon>("ticket_10661_2",
            a_min_b, ticket_10661[2],
            1, 8, 825192.0,
            1, 10, 27226370.5,
            1, -1, 825192.0 + 27226370.5,
            settings);
    }
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double > >();

    test_specific<bg::model::d2::point_xy<int>, false, false>();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<bg::model::d2::point_xy<float> >();

#if defined(HAVE_TTMATH)
    std::cout << "Testing TTMATH" << std::endl;
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

#endif

    return 0;
}
