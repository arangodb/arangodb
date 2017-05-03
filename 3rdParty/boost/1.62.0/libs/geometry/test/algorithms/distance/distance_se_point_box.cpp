// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <iostream>

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_spherical_equatorial_point_box
#endif

#include <boost/range.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/test/included/unit_test.hpp>
#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include "test_distance_se_common.hpp"


typedef bg::cs::spherical_equatorial<bg::degree> cs_type;
typedef bg::model::point<double, 2, cs_type> point_type;
typedef bg::model::segment<point_type> segment_type;
typedef bg::model::box<point_type> box_type;

namespace distance = bg::strategy::distance;
namespace services = distance::services;
typedef bg::default_distance_result<point_type, point_type>::type return_type;

typedef distance::cross_track_point_box<> point_box_strategy;
typedef distance::cross_track_point_box
    <
        void, distance::comparable::cross_track<>
    > comparable_point_box_strategy;

//===========================================================================

inline bg::distance_result
    <
        point_type, point_type, distance::haversine<double>
    >::type
distance_pp(std::string const& wkt1,
            std::string const& wkt2,
            double radius)
{
    point_type p1, p2;
    bg::read_wkt(wkt1, p1);
    bg::read_wkt(wkt2, p2);

    distance::haversine<double> strategy(radius);
    return bg::distance(p1, p2, strategy);
}

inline bg::default_comparable_distance_result<point_type>::type
comparable_distance_pp(std::string const& wkt1,
                       std::string const& wkt2)
{
    point_type p1, p2;
    bg::read_wkt(wkt1, p1);
    bg::read_wkt(wkt2, p2);
    return bg::comparable_distance(p1, p2);
}

inline bg::distance_result
    <
        point_type, point_type, distance::cross_track<>
    >::type
distance_ps(std::string const& wkt_point,
            std::string const& wkt_segment,
            double radius)
{
    point_type point;
    segment_type segment;
    bg::read_wkt(wkt_point, point);
    bg::read_wkt(wkt_segment, segment);

    distance::cross_track<> strategy(radius);
    return bg::distance(point, segment, strategy);
}

inline bg::default_comparable_distance_result<point_type>::type
comparable_distance_ps(std::string const& wkt_point,
                       std::string const& wkt_segment)
{
    point_type point;
    segment_type segment;
    bg::read_wkt(wkt_point, point);
    bg::read_wkt(wkt_segment, segment);
    return bg::comparable_distance(point, segment);
}

enum features_type { pp, ps };

template <typename Geometry1, typename Geometry2>
struct test_distances
{
    template <typename Strategy>
    static inline void apply(std::string const& case_id,
                             std::string const& wkt1,
                             std::string const& wkt2,
                             double expected_distance,
                             double expected_comparable_distance,
                             Strategy const& strategy)
    {
        typedef test_distance_of_geometries<Geometry1, Geometry2> tester;

        bool const is_comparable = boost::is_same
            <
                Strategy,
                typename services::comparable_type<Strategy>::type
            >::value;

        if (BOOST_GEOMETRY_CONDITION(is_comparable))
        {
            tester::apply(case_id, wkt1, wkt2,
                          expected_comparable_distance,
                          expected_comparable_distance,
                          strategy);
        }
        else
        {
            tester::apply(case_id, wkt1, wkt2,
                          expected_distance,
                          expected_comparable_distance,
                          strategy);
        }
    }

    template <typename Strategy>
    static inline void apply(std::string const& case_id,
                             std::string const& wkt1,
                             std::string const& wkt2,
                             std::string const& feature1,
                             std::string const& feature2,
                             features_type ftype,
                             Strategy const& strategy)
    {
        double const radius = strategy.radius();
        double expected_distance, expected_comparable_distance;

        if (ftype == pp)
        {
            expected_distance = distance_pp(feature1, feature2, radius);
            expected_comparable_distance
                = comparable_distance_pp(feature1, feature2);
        }
        else
        {
            expected_distance = distance_ps(feature1, feature2, radius);
            expected_comparable_distance
                = comparable_distance_ps(feature1, feature2);
        }

        apply(case_id, wkt1, wkt2,
              expected_distance, expected_comparable_distance,
              strategy);
    }
};

template <typename T, typename U>
T to_comparable(T const& value, U const& radius)
{
    T x = sin(value / (radius * 2.0));
    return x * x;
}

//===========================================================================

template <typename Strategy>
void test_distance_point_box(Strategy const& strategy)
{
#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << "point/box distance tests" << std::endl;
#endif
    typedef test_distances<point_type, box_type> tester;

    double const radius = strategy.radius();
    double const d2r = bg::math::d2r<double>();

    // Cases for relative location of a point wrt to a box
    //
    //           |         |
    //           |    3    |
    //           |         |
    //           +---------+
    //           |         |
    //       1   |    5    |   2
    //           |         |
    //           +---------+
    //           |         |
    //           |    4    |
    //           |         |
    //
    // and also the following cases
    //
    //           |         |
    //           A         B
    //           |         |
    //           +----C----+
    //           |         |
    //           D         E
    //           |         |
    //           +----F----+
    //           |         |
    //           G         H
    //           |         |
    //
    // and finally we have the corners
    //
    //           |         |
    //           |         |
    //           |         |
    //           a---------b
    //           |         |
    //           |         |
    //           |         |
    //           c---------d
    //           |         |
    //           |         |
    //           |         |
    //
    // for each relative position we also have to test the shifted point
    // (this is due to the fact that boxes have longitudes in the
    // range [-180, 540)

    std::string const box1 = "BOX(10 10,20 20)";

    // case 1
    tester::apply("pb1-1a", "POINT(5 25)", box1,
                  "POINT(5 25)", "POINT(10 20)", pp,
                  strategy);

    // case 1
    tester::apply("pb1-1b", "POINT(3 12)", box1,
                  "POINT(3 12)", "SEGMENT(10 10,10 20)", ps,
                  strategy);

    // case 1
    tester::apply("pb1-1c", "POINT(3 17)", box1,
                  "POINT(3 17)", "SEGMENT(10 10,10 20)", ps,
                  strategy);

    // case 1
    tester::apply("pb1-1d", "POINT(5 4)", box1,
                  "POINT(5 4)", "POINT(10 10)", pp,
                  strategy);

    // case 1
    tester::apply("pb1-1e", "POINT(-100 20)", box1,
                  "POINT(-100 20)", "POINT(10 20)", pp,
                  strategy);

    // case 1
    tester::apply("pb1-1g", "POINT(-100 10)", box1,
                  "POINT(-100 10)", "SEGMENT(10 10,10 20)", ps,
                  strategy);

    // case 2
    tester::apply("pb1-2a", "POINT(31 25)", box1,
                  "POINT(31 25)", "POINT(20 20)", pp,
                  strategy);

    // case 2
    tester::apply("pb1-2b", "POINT(23 17)", box1,
                  "POINT(23 17)", "SEGMENT(20 10,20 20)", ps,
                  strategy);

    // case 2
    tester::apply("pb1-2c", "POINT(29 3)", box1,
                  "POINT(29 3)", "POINT(20 10)", pp,
                  strategy);

    // case 2
    tester::apply("pb1-2d", "POINT(131 65)", box1,
                  "POINT(131 65)", "POINT(20 20)", pp,
                  strategy);

    // case 2
    tester::apply("pb1-2e", "POINT(110 10)", box1,
                  "POINT(110 10)", "SEGMENT(20 10,20 20)", ps,
                  strategy);

    // case 2
    tester::apply("pb1-2f", "POINT(150 20)", box1,
                  "POINT(150 20)", "POINT(20 20)", pp,
                  strategy);

    // case 3
    tester::apply("pb1-3a", "POINT(11 25)", box1,
                  5.0 * d2r * radius,
                  to_comparable(5.0 * d2r * radius, radius),
                  strategy);

    // case 3
    tester::apply("pb1-3b", "POINT(15 25)", box1,
                  5.0 * d2r * radius,
                  to_comparable(5.0 * d2r * radius, radius),
                  strategy);

    // case 3
    tester::apply("pb1-3c", "POINT(18 25)", box1,
                  5.0 * d2r * radius,
                  to_comparable(5.0 * d2r * radius, radius),
                  strategy);

    // case 4
    tester::apply("pb1-4a", "POINT(13 4)", box1,
                  6.0 * radius * d2r,
                  to_comparable(6.0 * radius * d2r, radius),
                  strategy);

    // case 4
    tester::apply("pb1-4b", "POINT(19 4)", box1,
                  6.0 * radius * d2r,
                  to_comparable(6.0 * radius * d2r, radius),
                  strategy);

    // case 5
    tester::apply("pb1-5", "POINT(15 14)", box1, 0, 0, strategy);

    // case A
    tester::apply("pb1-A", "POINT(10 28)", box1,
                  8.0 * d2r * radius,
                  to_comparable(8.0 * d2r * radius, radius),
                  strategy);

    // case B
    tester::apply("pb1-B", "POINT(20 28)", box1,
                  8.0 * d2r * radius,
                  to_comparable(8.0 * d2r * radius, radius),
                  strategy);

    // case C
    tester::apply("pb1-C", "POINT(14 20)", box1, 0, 0, strategy);

    // case D
    tester::apply("pb1-D", "POINT(10 17)", box1, 0, 0, strategy);

    // case E
    tester::apply("pb1-E", "POINT(20 11)", box1, 0, 0, strategy);

    // case F
    tester::apply("pb1-F", "POINT(19 10)", box1, 0, 0, strategy);

    // case G
    tester::apply("pb1-G", "POINT(10 -40)", box1,
                  50.0 * d2r * radius,
                  to_comparable(50.0 * d2r * radius, radius),
                  strategy);

    // case H
    tester::apply("pb1-H",
                  "POINT(20 -50)", box1,
                  60.0 * d2r * radius,
                  to_comparable(60.0 * d2r * radius, radius),
                  strategy);

    // case a
    tester::apply("pb1-a", "POINT(10 20)", box1, 0, 0, strategy);
    // case b
    tester::apply("pb1-b", "POINT(20 20)", box1, 0, 0, strategy);
    // case c
    tester::apply("pb1-c", "POINT(10 10)", box1, 0, 0, strategy);
    // case d
    tester::apply("pb1-d", "POINT(20 10)", box1, 0, 0, strategy);



    std::string const box2 = "BOX(170 -60,400 80)";

    // case 1 - point is closer to western meridian
    tester::apply("pb2-1a", "POINT(160 0)", box2,
                  "POINT(160 0)", "SEGMENT(170 -60,170 80)", ps,
                  strategy);

    // case 1 - point is closer to eastern meridian
    tester::apply("pb2-1b", "POINT(50 0)", box2,
                  "POINT(50 0)", "SEGMENT(40 -60,40 80)", ps,
                  strategy);

    // case 3 - equivalent point POINT(390 85) is above the box
    tester::apply("pb2-3", "POINT(30 85)", box2,
                  5.0 * d2r * radius,
                  to_comparable(5.0 * d2r * radius, radius),
                  strategy);

    // case 4 - equivalent point POINT(390 -75) is below the box
    tester::apply("pb2-4", "POINT(30 -75)", box2,
                  15.0 * d2r * radius,
                  to_comparable(15.0 * d2r * radius, radius),
                  strategy);

    // case 5 - equivalent point POINT(390 0) is inside box
    tester::apply("pb2-5", "POINT(30 0)", box2, 0, 0, strategy);


    std::string const box3 = "BOX(-150 -50,-40 70)";

    // case 1 - point is closer to western meridian
    tester::apply("pb3-1a", "POINT(-170 10)", box3,
                  "POINT(-170 10)", "SEGMENT(-150 -50,-150 70)", ps,
                  strategy);

    // case 2 - point is closer to eastern meridian
    tester::apply("pb3-2a", "POINT(5 10)", box3,
                  "POINT(5 10)", "SEGMENT(-40 -50,-40 70)", ps,
                  strategy);

    // case 2 - point is closer to western meridian
    tester::apply("pb3-2a", "POINT(160 10)", box3,
                  "POINT(160 10)", "SEGMENT(-150 -50,-150 70)", ps,
                  strategy);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb3-2c1", "POINT(85 20)", box3,
                  "POINT(85 20)", "SEGMENT(-150 -50,-150 70)", ps,
                  strategy);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb3-2c2", "POINT(85 20)", box3,
                  "POINT(85 20)", "SEGMENT(-40 -50,-40 70)", ps,
                  strategy);

    // box that is symmetric wrt the prime meridian
    std::string const box4 = "BOX(-75 -45,75 65)";

    // case 1 - point is closer to western meridian
    tester::apply("pb4-1a", "POINT(-100 10)", box4,
                  "POINT(-100 10)", "SEGMENT(-75 -45,-75 65)", ps,
                  strategy);

    // case 2 - point is closer to eastern meridian
    tester::apply("pb4-2a", "POINT(90 15)", box4,
                  "POINT(90 15)", "SEGMENT(75 -45,75 65)", ps,
                  strategy);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb4-2c1", "POINT(-180 20)", box4,
                  "POINT(-180 20)", "SEGMENT(-75 -45,-75 65)", ps,
                  strategy);

    // case 2 - point is at equal distance from eastern and western meridian
    tester::apply("pb4-2c2", "POINT(-180 20)", box4,
                  "POINT(-180 20)", "SEGMENT(75 -45,75 65)", ps,
                  strategy);
}

BOOST_AUTO_TEST_CASE( test_point_box )
{
    test_distance_point_box(point_box_strategy());
    test_distance_point_box(point_box_strategy(earth_radius_km));
    test_distance_point_box(point_box_strategy(earth_radius_miles));

    test_distance_point_box(comparable_point_box_strategy());
    test_distance_point_box(comparable_point_box_strategy(earth_radius_km));
    test_distance_point_box(comparable_point_box_strategy(earth_radius_miles));
}
