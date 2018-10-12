// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2018 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_formula.hpp"
#include "direct_cases.hpp"

#include <boost/geometry/formulas/vincenty_direct.hpp>
#include <boost/geometry/formulas/thomas_direct.hpp>
//#include <boost/geometry/formulas/series_expansion_direct.hpp>
#include <boost/geometry/formulas/spherical.hpp>

#include <boost/geometry/srs/srs.hpp>

template <typename Result>
void check_direct(Result const& result, expected_result const& expected, expected_result const& reference, double reference_error)
{
    check_direct_sph(result, expected, reference, reference_error);
    check_one(result.reduced_length, expected.reduced_length, reference.reduced_length, reference_error);
    check_one(result.geodesic_scale, expected.geodesic_scale, reference.geodesic_scale, reference_error);
}

template <typename Result>
void check_direct_sph(Result const& result, expected_result const& expected, expected_result const& reference, double reference_error)
{
    check_one(result.lon2, expected.lon2, reference.lon2, reference_error, true);
    check_one(result.lat2, expected.lat2, reference.lat2, reference_error, true);
    check_one(result.reverse_azimuth, expected.reverse_azimuth, reference.reverse_azimuth, reference_error, true);
}

void test_all(expected_results const& results)
{
    double const d2r = bg::math::d2r<double>();
    double const r2d = bg::math::r2d<double>();

    double lon1r = results.p1.lon * d2r;
    double lat1r = results.p1.lat * d2r;
    double distance = results.distance;
    double azi12r = results.azimuth12 * d2r;

    // WGS84
    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);
    bg::srs::sphere<double> const sphere;

    bg::formula::result_direct<double> result;

    typedef bg::formula::vincenty_direct<double, true, true, true, true> vi_t;
    result = vi_t::apply(lon1r, lat1r, distance, azi12r, spheroid);
    result.lon2 *= r2d;
    result.lat2 *= r2d;
    result.reverse_azimuth *= r2d;
    check_direct(result, results.vincenty, results.karney, 0.00000001);

    typedef bg::formula::thomas_direct<double, true, true, true, true, true> th_t;
    result = th_t::apply(lon1r, lat1r, distance, azi12r, spheroid);
    result.lon2 *= r2d;
    result.lat2 *= r2d;
    result.reverse_azimuth *= r2d;
    check_direct(result, results.thomas, results.karney, 0.0000001);

    typedef bg::formula::thomas_direct<double, false, true, true, true, true> th_t1st;
    result = th_t1st::apply(lon1r, lat1r, distance, azi12r, spheroid);
    result.lon2 *= r2d;
    result.lat2 *= r2d;
    result.reverse_azimuth *= r2d;
    check_direct(result, results.thomas1st, results.karney, 0.0000001);
/*
    typedef bg::formula::series_expansion_direct<double, true, true, true, true, 4> series;
    result = series::apply(lon1r, lat1r, distance, azi12r, spheroid);
    result.lon2 *= r2d;
    result.lat2 *= r2d;
    result.reverse_azimuth *= r2d;
    check_direct(result, results.series, results.karney, 0.0000001);
*/
    result = bg::formula::spherical_direct<true, true>(lon1r, lat1r, distance,
                                                       azi12r, sphere);
    result.lon2 *= r2d;
    result.lat2 *= r2d;
    result.reverse_azimuth *= r2d;
    check_direct_sph(result, results.spherical, results.karney, 0.1);
}

int test_main(int, char*[])
{
    for (size_t i = 0; i < expected_size; ++i)
    {
        test_all(expected[i]);
    }

    return 0;
}
