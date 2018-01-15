// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <test_geometries/custom_lon_lat_point.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>


namespace bg = boost::geometry;


template <typename CoordinateSystem>
inline void test_coordinate_system()
{
    typedef bg::model::point<double, 2, CoordinateSystem> bg_double_point;
    typedef bg::model::point<int, 2, CoordinateSystem> bg_int_point;

    typedef rw_lon_lat_point<double, CoordinateSystem> rw_double_point;
    typedef ro_lon_lat_point<double, CoordinateSystem> ro_double_point;

    typedef rw_lon_lat_point<int, CoordinateSystem> rw_int_point;
    typedef ro_lon_lat_point<int, CoordinateSystem> ro_int_point;

    bg::concepts::check<bg_int_point>();
    bg::concepts::check<bg_int_point const>();

    bg::concepts::check<bg_double_point>();
    bg::concepts::check<bg_double_point const>();

    bg::concepts::check<rw_int_point>();
    bg::concepts::check<rw_int_point const>();
    bg::concepts::check<ro_int_point const>();

    bg::concepts::check<rw_double_point>();
    bg::concepts::check<rw_double_point const>();
    bg::concepts::check<ro_double_point const>();
}


int main()
{
    test_coordinate_system<bg::cs::geographic<bg::degree> >();
    test_coordinate_system<bg::cs::geographic<bg::radian> >();

    test_coordinate_system<bg::cs::spherical<bg::degree> >();
    test_coordinate_system<bg::cs::spherical<bg::radian> >();

    test_coordinate_system<bg::cs::spherical_equatorial<bg::degree> >();
    test_coordinate_system<bg::cs::spherical_equatorial<bg::radian> >();

    test_coordinate_system<bg::cs::polar<bg::degree> >();
    test_coordinate_system<bg::cs::polar<bg::radian> >();

    return 0;
}
