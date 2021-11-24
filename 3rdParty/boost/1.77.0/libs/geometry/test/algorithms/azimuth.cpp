// Boost.Geometry
// Unit Test

// Copyright (c) 2021, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#include <geometry_test_common.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/algorithms/azimuth.hpp>


template <typename P, typename S>
auto call_azimuth(P const& p1, P const& p2, S const& s)
{
    return bg::azimuth(p1, p2, s);
}

template <typename P>
auto call_azimuth(P const& p1, P const& p2, bg::default_strategy const&)
{
    return bg::azimuth(p1, p2);
}

template <typename P, typename S = bg::default_strategy>
void test_one(P const& p1, P const& p2, double expected, S const& s = S())
{
    double const result = call_azimuth(p1, p2, s) * bg::math::r2d<double>();
    BOOST_CHECK_CLOSE(result, expected, 0.0001);
}

template <typename P>
void test_car()
{
    test_one(P(0, 0), P(0, 0), 0);
    test_one(P(0, 0), P(1, 1), 45);
    test_one(P(0, 0), P(100, 1), 89.427061302316517);
    test_one(P(0, 0), P(-1, 1), -45);
    test_one(P(0, 0), P(-100, 1), -89.427061302316517);
}

template <typename P>
void test_sph()
{
    test_one(P(0, 0), P(0, 0), 0);
    test_one(P(0, 0), P(1, 1), 44.995636455344851);
    test_one(P(0, 0), P(100, 1), 88.984576593576293);
    test_one(P(0, 0), P(-1, 1), -44.995636455344851);
    test_one(P(0, 0), P(-100, 1), -88.984576593576293);
}

template <typename P>
void test_geo()
{
    test_one(P(0, 0), P(0, 0), 0);
    test_one(P(0, 0), P(1, 1), 45.187718848049521);
    test_one(P(0, 0), P(100, 1), 88.986933066023497);
    test_one(P(0, 0), P(-1, 1), -45.187718848049521);
    test_one(P(0, 0), P(-100, 1), -88.986933066023497);
}

template <typename P>
void test_geo_v()
{
    bg::strategies::azimuth::geographic<bg::strategy::vincenty> s;

    test_one(P(0, 0), P(0, 0), 0, s);
    test_one(P(0, 0), P(1, 1), 45.188040229339755, s);
    test_one(P(0, 0), P(100, 1), 88.986914518230208, s);
    test_one(P(0, 0), P(-1, 1), -45.188040229339755, s);
    test_one(P(0, 0), P(-100, 1), -88.986914518230208, s);
}

int test_main(int, char* [])
{
    test_car< bg::model::point<double, 2, bg::cs::cartesian> >();
    test_sph< bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_geo< bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();
    test_geo_v< bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();
    
    return 0;
}
