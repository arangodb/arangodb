// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2019 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2020.
// Modifications copyright (c) 2020 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_buffer.hpp"

static std::string const simplex = "MULTIPOINT((5 5),(7 7))";
static std::string const three = "MULTIPOINT((5 8),(9 8),(7 11))";

// Generated error (extra polygon on top of rest) at distance 14.0:
static std::string const multipoint_a = "MULTIPOINT((39 44),(38 37),(41 29),(15 33),(58 39))";

// Just one with holes at distance ~ 15
static std::string const multipoint_b = "MULTIPOINT((5 56),(98 67),(20 7),(58 60),(10 4),(75 68),(61 68),(75 62),(92 26),(74 6),(67 54),(20 43),(63 30),(45 7))";

// Grid, U-form, generates error for square point at 0.54 (top cells to control rescale)
static std::string const grid_a = "MULTIPOINT(5 0,6 0,7 0,  5 1,7 1,  0 13,8 13)";

static std::string const mysql_report_2015_02_25_1 = "MULTIPOINT(-9 19,9 -6,-4 4,16 -14,-3 16,14 9)";
static std::string const mysql_report_2015_02_25_2 = "MULTIPOINT(-2 11,-15 3,6 4,-14 0,20 -7,-17 -1)";

static std::string const mysql_report_3 = "MULTIPOINT(0 0,0 0,0 0,0 0,0 0)";

template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::polygon<P, Clockwise> polygon;
    typedef bg::model::multi_point<P> multi_point_type;

    bg::strategy::buffer::join_round join;
    bg::strategy::buffer::end_flat end_flat;
    typedef bg::strategy::buffer::distance_symmetric
    <
        typename bg::coordinate_type<P>::type
    > distance_strategy;
    bg::strategy::buffer::side_straight side_strategy;

    double const expectation = boost::geometry::math::pi<double>() *  0.99915;

    test_one<multi_point_type, polygon>("simplex1", simplex, join, end_flat, 2.0 * expectation, 1.0);
    test_one<multi_point_type, polygon>("simplex2", simplex, join, end_flat, 22.8335, 2.0);
    test_one<multi_point_type, polygon>("simplex3", simplex, join, end_flat, 44.5619, 3.0);

    test_one<multi_point_type, polygon>("three1", three, join, end_flat, 3.0 * expectation, 1.0);
#if defined(BOOST_GEOMETRY_USE_RESCALING) || defined(BOOST_GEOMETRY_TEST_FAILURES)
    // For no-rescaling, fails in CCW mode
    test_one<multi_point_type, polygon>("three2", three, join, end_flat, 36.7528, 2.0);
#endif
    test_one<multi_point_type, polygon>("three19", three, join, end_flat, 33.6857, 1.9);
    test_one<multi_point_type, polygon>("three21", three, join, end_flat, 39.6337, 2.1);
    test_one<multi_point_type, polygon>("three3", three, join, end_flat, 65.5243, 3.0);

    test_one<multi_point_type, polygon>("multipoint_a", multipoint_a, join, end_flat, 2049.98, 14.0);
    test_one<multi_point_type, polygon>("multipoint_b", multipoint_b, join, end_flat, 7109.88, 15.0);
    test_one<multi_point_type, polygon>("multipoint_b1", multipoint_b, join, end_flat, 6911.89, 14.7);
    test_one<multi_point_type, polygon>("multipoint_b2", multipoint_b, join, end_flat, 7174.79, 15.1);

    // Grid tests
    {
        bg::strategy::buffer::point_square point_strategy;

        test_with_custom_strategies<multi_point_type, polygon>("grid_a50",
                grid_a, join, end_flat,
                distance_strategy(0.5), side_strategy, point_strategy, 7.0);

        test_with_custom_strategies<multi_point_type, polygon>("grid_a54",
                grid_a, join, end_flat,
                distance_strategy(0.54), side_strategy, point_strategy, 7.819);
    }

    test_with_custom_strategies<multi_point_type, polygon>("mysql_report_2015_02_25_1_800",
            mysql_report_2015_02_25_1, join, end_flat,
            distance_strategy(6051788), side_strategy,
            bg::strategy::buffer::point_circle(800),
            115057490003226.125, ut_settings(1.0));

    {
        typename bg::strategies::relate::services::default_strategy
            <
                multi_point_type, multi_point_type
            >::type strategy;

        multi_point_type g;
        bg::read_wkt(mysql_report_3, g);
        bg::model::multi_polygon<polygon> buffered;
        test_buffer<polygon>("mysql_report_3", buffered, g,
            bg::strategy::buffer::join_round(36),
            bg::strategy::buffer::end_round(36),
            distance_strategy(1),
            side_strategy,
            bg::strategy::buffer::point_circle(36),
            strategy,
            1, 0, 3.12566719800474635, ut_settings(1.0));
    }
}

template <typename P>
void test_many_points_per_circle()
{
    // Tests for large distances / many points in circles.
    // Before Boost 1.58, this would (seem to) hang. It is solved by using monotonic sections in get_turns for buffer
    // This is more time consuming, only calculate this for counter clockwise
    // Reported by MySQL 2015-02-25
    //   SELECT ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT(''), 6051788, ST_BUFFER_STRATEGY('point_circle', 83585)));
    //   SELECT ST_ASTEXT(ST_BUFFER(ST_GEOMFROMTEXT(''), 5666962, ST_BUFFER_STRATEGY('point_circle', 46641))) ;

    typedef bg::model::polygon<P, false> polygon;
    typedef bg::model::multi_point<P> multi_point_type;

    bg::strategy::buffer::join_round join;
    bg::strategy::buffer::end_flat end_flat;
    typedef bg::strategy::buffer::distance_symmetric
    <
        typename bg::coordinate_type<P>::type
    > distance_strategy;
    bg::strategy::buffer::side_straight side_strategy;

    using bg::strategy::buffer::point_circle;

#if ! defined(BOOST_GEOMETRY_USE_RESCALING)
    double const tolerance = 1000.0;
#else
    double const tolerance = 1.0;
#endif

    // Area should be somewhat larger (~>) than pi*distance^2
    // 6051788: area ~> 115058122875258

    // Strategies with many points, which are (very) slow in debug mode
    test_with_custom_strategies<multi_point_type, polygon>(
            "mysql_report_2015_02_25_1_8000",
            mysql_report_2015_02_25_1, join, end_flat,
            distance_strategy(6051788), side_strategy, point_circle(8000),
            115058661065242.812, ut_settings(10.0 * tolerance));

    // Expectations:
    // 115058672785641.031
    // 115058672785680.281
    // 115058672785679.922
    test_with_custom_strategies<multi_point_type, polygon>(
            "mysql_report_2015_02_25_1",
            mysql_report_2015_02_25_1, join, end_flat,
            distance_strategy(6051788), side_strategy, point_circle(83585),
            115058672785660.0, ut_settings(25.0 * tolerance));

    // Takes about 7 seconds in release mode
    // Expectations:
    // 115058672880035.391
    // 115058672879944.547
    // 115058672879920.484
    test_with_custom_strategies<multi_point_type, polygon>(
            "mysql_report_2015_02_25_1_250k",
            mysql_report_2015_02_25_1, join, end_flat,
            distance_strategy(6051788), side_strategy, point_circle(250000),
            115058672879977.0, ut_settings(150.0 * tolerance));

#if defined(BOOST_GEOMETRY_BUFFER_INCLUDE_SLOW_TESTS)
    // Takes about 110 seconds in release mode
    test_with_custom_strategies<multi_point_type, polygon>(
            "mysql_report_2015_02_25_1_800k",
            mysql_report_2015_02_25_1, join, end_flat,
            distance_strategy(6051788), side_strategy, point_circle(800000),
            115058672871849.219, ut_settings(tolerance));
#endif

    // 5666962: area ~> 100890546298964
    // Expectations:
    // 100891031341796.875
    // 100891031341794.766
    // 100891031341794.078
    test_with_custom_strategies<multi_point_type, polygon>(
            "mysql_report_2015_02_25_2",
            mysql_report_2015_02_25_2, join, end_flat,
            distance_strategy(5666962), side_strategy, point_circle(46641),
            100891031341795.0, ut_settings(200.0 * tolerance));

    // Multipoint b with large distances/many points
    // Area ~> pi * 10x

    // Expectations:
    // 3141871558222.398
    // 3141871558231.5166
    // 3141871558231.48926

    test_with_custom_strategies<multi_point_type, polygon>(
            "multipoint_b_50k",
            multipoint_b, join, end_flat,
            distance_strategy(1000000), side_strategy, point_circle(50000),
            3141871558227.0, ut_settings(40.0 * tolerance));

#if defined(BOOST_GEOMETRY_BUFFER_INCLUDE_SLOW_TESTS)
    // Tests optimization min/max radius
    // Takes about 55 seconds in release mode
    test_with_custom_strategies<multi_point_type, polygon>(
            "multipoint_b_500k",
            multipoint_b, join, end_flat,
            distance_strategy(10000000), side_strategy, point_circle(500000),
            314162054419515.562, ut_settings((tolerance));
#endif
}

int test_main(int, char* [])
{
    BoostGeometryWriteTestConfiguration();

    test_all<true, bg::model::point<default_test_type, 2, bg::cs::cartesian> >();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_ORDER)
    test_all<false, bg::model::point<default_test_type, 2, bg::cs::cartesian> >();
#endif

#if defined(BOOST_GEOMETRY_COMPILER_MODE_RELEASE) && ! defined(BOOST_GEOMETRY_COMPILER_MODE_DEBUG)
    test_many_points_per_circle<bg::model::point<double, 2, bg::cs::cartesian> >();
#else
    std::cout << "Skipping some tests in debug or unknown mode" << std::endl;
#endif

#if defined(BOOST_GEOMETRY_TEST_FAILURES)
    BoostGeometryWriteExpectedFailures(BG_NO_FAILURES);
#endif

    return 0;
}
