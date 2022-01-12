// Boost.Geometry
// Unit Test Helper

// Copyright (c) 2018-2019 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2020.
// Modifications copyright (c) 2020 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef BOOST_GEOMETRY_TEST_BUFFER_GEO_HPP
#define BOOST_GEOMETRY_TEST_BUFFER_GEO_HPP

#include "test_buffer.hpp"

template
<
    typename Geometry,
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy
>
void test_one_geo(std::string const& caseid,
        std::string const& wkt,
        JoinStrategy const& join_strategy, EndStrategy const& end_strategy,
        int expected_count, int expected_holes_count, double expected_area,
        double distance_left, ut_settings settings = ut_settings(),
        double distance_right = same_distance)
{
    Geometry input_geometry;
    bg::read_wkt(wkt, input_geometry);
    bg::correct(input_geometry);

    bg::strategy::buffer::side_straight side_strategy;
    bg::strategy::buffer::distance_asymmetric
    <
        typename bg::coordinate_type<Geometry>::type
    > distance_strategy(distance_left,
                        bg::math::equals(distance_right, same_distance)
                        ? distance_left : distance_right);

    // Use the appropriate strategy for geographic points
    bg::strategy::buffer::geographic_point_circle<> circle_strategy(settings.points_per_circle);

    // Use Thomas strategy to calculate geographic area, because it is
    // the most precise (unless scale of buffer is only around 1 meter)
    // TODO: If area is for calculation of the orientation of points in a ring
    //   and accuracy is an issue, then instead calculate_point_order should
    //   probably be used instead of area.
    bg::strategies::relate::geographic
        <
            bg::strategy::thomas, bg::srs::spheroid<long double>, long double
        > strategy;

    bg::model::multi_polygon<GeometryOut> buffer;

    test_buffer<GeometryOut>
            (caseid, buffer, input_geometry,
            join_strategy, end_strategy,
            distance_strategy, side_strategy, circle_strategy,
            strategy,
            expected_count, expected_holes_count, expected_area,
            settings);
}

template
<
    typename Geometry,
    typename GeometryOut,
    typename JoinStrategy,
    typename EndStrategy
>
void test_one_geo(std::string const& caseid, std::string const& wkt,
        JoinStrategy const& join_strategy, EndStrategy const& end_strategy,
        double expected_area,
        double distance_left, ut_settings const& settings = ut_settings(),
        double distance_right = same_distance)
{
    test_one_geo<Geometry, GeometryOut>(caseid, wkt, join_strategy, end_strategy,
        -1 ,-1, expected_area,
        distance_left, settings, distance_right);
}


#endif
