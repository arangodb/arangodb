// Boost.Geometry
// Unit Test

// Copyright (c) 2018 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_GEOMETRY_NORMALIZE_LATITUDE

#include <sstream>

#include "test_formula.hpp"
#include "direct_meridian_cases.hpp"

#include <boost/geometry/srs/srs.hpp>
#include <boost/geometry/formulas/vincenty_direct.hpp>
#include <boost/geometry/formulas/meridian_direct.hpp>

template <typename Result, typename Expected, typename Reference>
void check_meridian_direct(Result& result,
                           Expected const& expected,
                           Reference& reference,
                           double reference_error)
{
    boost::geometry::math::normalize_spheroidal_coordinates
        <
            boost::geometry::radian,
            double
        >(result.lon2, result.lat2);

    boost::geometry::math::normalize_spheroidal_coordinates
        <
            boost::geometry::radian,
            double
        >(reference.lon2, reference.lat2);

    std::stringstream ss;
    ss << "(" << result.lon2 * bg::math::r2d<double>()
       << " " << result.lat2 * bg::math::r2d<double>() << ")";

    check_one("lon:" + ss.str(), result.lon2, expected.lon, reference.lon2,
              reference_error);
    check_one("lat:" + ss.str(), result.lat2, expected.lat, reference.lat2,
              reference_error);
    check_one("rev_az:" + ss.str(), result.reverse_azimuth,
              result.reverse_azimuth, reference.reverse_azimuth, reference_error);
    check_one("red len:" + ss.str(), result.reduced_length, result.reduced_length,
              reference.reduced_length, 0.01);
    check_one("geo scale:" + ss.str(), result.geodesic_scale, result.geodesic_scale,
              reference.geodesic_scale, 0.01);

}

void test_all(expected_results const& results)
{
    double const d2r = bg::math::d2r<double>();

    double lon1_rad = results.p1.lon * d2r;
    double lat1_rad = results.p1.lat * d2r;
    coordinates expected_point;
    expected_point.lon = results.p2.lon * d2r;
    expected_point.lat = results.p2.lat * d2r;
    double distance = results.distance;
    bool direction = results.direction;

    // WGS84
    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);

    bg::formula::result_direct<double> vincenty_result;
    bg::formula::result_direct<double> meridian_result;

    typedef bg::formula::vincenty_direct<double, true, true, true, true> vi_t;
    double vincenty_azimuth = direction ? 0.0 : bg::math::pi<double>();
    vincenty_result = vi_t::apply(lon1_rad, lat1_rad, distance, vincenty_azimuth, spheroid);

    {
        typedef bg::formula::meridian_direct<double, true, true, true, true, 1> eli;
        meridian_result = eli::apply(lon1_rad, lat1_rad, distance, direction, spheroid);
        check_meridian_direct(meridian_result, expected_point, vincenty_result, 0.001);
    }
    {
        typedef bg::formula::meridian_direct<double, true, true, true, true, 2> eli;
        meridian_result = eli::apply(lon1_rad, lat1_rad, distance, direction, spheroid);
        check_meridian_direct(meridian_result, expected_point, vincenty_result, 0.00001);
    }
    {
        typedef bg::formula::meridian_direct<double, true, true, true, true, 3> eli;
        meridian_result = eli::apply(lon1_rad, lat1_rad, distance, direction, spheroid);
        check_meridian_direct(meridian_result, expected_point, vincenty_result, 0.00000001);
    }
    {
        typedef bg::formula::meridian_direct<double, true, true, true, true, 4> eli;
        meridian_result = eli::apply(lon1_rad, lat1_rad, distance, direction, spheroid);
        check_meridian_direct(meridian_result, expected_point, vincenty_result, 0.00000001);
    }
    {
        typedef bg::formula::meridian_direct<double, true, true, true, true, 5> eli;
        meridian_result = eli::apply(lon1_rad, lat1_rad, distance, direction, spheroid);
        check_meridian_direct(meridian_result, expected_point, vincenty_result, 0.00000000001);
    }
}

int test_main(int, char*[])
{
    for (size_t i = 0; i < expected_size; ++i)
    {
        test_all(expected[i]);
    }

    return 0;
}
