// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2016 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithms/test_perimeter.hpp>
#include <algorithms/perimeter/perimeter_polygon_cases.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

template <typename P>
void test_all_default() //test the default strategy
{
    double const pi = boost::math::constants::pi<double>();

    for (std::size_t i = 0; i <= 2; ++i)
    {
        test_geometry<bg::model::polygon<P> >(poly_data_sph[i], 2 * pi);
    }

    // Multipolygon
    test_geometry<bg::model::multi_polygon<bg::model::polygon<P> > >
                                            (multipoly_data[0], 3 * pi);

    // Geometries with length zero
    test_geometry<P>("POINT(0 0)", 0);
    test_geometry<bg::model::linestring<P> >("LINESTRING(0 0,3 4,4 3)", 0);
}


template <typename P>
void test_all_haversine(double const mean_radius)
{
    double const pi = boost::math::constants::pi<double>();
    bg::strategy::distance::haversine<double> haversine_strategy(mean_radius);

    for (std::size_t i = 0; i <= 2; ++i)
    {
        test_geometry<bg::model::polygon<P> >(poly_data_sph[i],
                                              2 * pi * mean_radius,
                                              haversine_strategy);
    }

    // Multipolygon
    test_geometry<bg::model::multi_polygon<bg::model::polygon<P> > >
                                            (multipoly_data[0],
                                             3 * pi * mean_radius,
                                             haversine_strategy);

    // Geometries with length zero
    test_geometry<P>("POINT(0 0)", 0, haversine_strategy);
    test_geometry<bg::model::linestring<P> >("LINESTRING(0 0,3 4,4 3)",
                                             0,
                                             haversine_strategy);
}

int test_main(int, char* [])
{
    //Earth radius estimation in Km
    //(see https://en.wikipedia.org/wiki/Earth_radius)
    double const mean_radius = 6371.0;

    test_all_default<bg::model::d2::point_xy<int,
            bg::cs::spherical_equatorial<bg::degree> > >();
    test_all_default<bg::model::d2::point_xy<float,
            bg::cs::spherical_equatorial<bg::degree> > >();
    test_all_default<bg::model::d2::point_xy<double,
            bg::cs::spherical_equatorial<bg::degree> > >();

    test_all_haversine<bg::model::d2::point_xy<int,
        bg::cs::spherical_equatorial<bg::degree> > >(mean_radius);
    test_all_haversine<bg::model::d2::point_xy<float,
        bg::cs::spherical_equatorial<bg::degree> > >(mean_radius);
    test_all_haversine<bg::model::d2::point_xy<double,
        bg::cs::spherical_equatorial<bg::degree> > >(mean_radius);

    return 0;
}
