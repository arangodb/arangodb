// Boost.Geometry
// Unit Test

// Copyright (c) 2019 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <sstream>

#include "../formulas/test_formula.hpp"
#include "distance_cross_track_cases.hpp"

#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/srs/spheroid.hpp>

struct error{
    double long_distance;
    double short_distance;
    double very_short_distance;
    double very_very_short_distance;
};

void check_result(double const& result, double const& expected,
                  double const& reference, error const& reference_error)
{
    BOOST_GEOMETRY_CHECK_CLOSE(result, expected, 0.0000001,
        std::setprecision(20) << "result {" << result << "} different than expected {" << expected << "}.");

    double reference_error_value = result > 2000 ? reference_error.long_distance
                                 : result > 100  ? reference_error.short_distance
                                 : result > 20   ? reference_error.very_short_distance
                                 : reference_error.very_very_short_distance;

    BOOST_GEOMETRY_CHECK_CLOSE(result, reference, reference_error_value,
        std::setprecision(20) << "result {" << result << "} different than reference {" << reference << "}.");
}

template <typename Point>
void test_all(expected_results const& results)
{
    double const d2r = bg::math::d2r<double>();

    double lon1r = results.p1.lon * d2r;
    double lat1r = results.p1.lat * d2r;
    double lon2r = results.p2.lon * d2r;
    double lat2r = results.p2.lat * d2r;
    double lon3r = results.p3.lon * d2r;
    double lat3r = results.p3.lat * d2r;

    typedef bg::srs::spheroid<double> Spheroid;

    // WGS84
    Spheroid spheroid(6378137.0, 6356752.3142451793);

    error errors [] =
    {
        {0.00000001, 0.00000001, 0.00000001, 0.000001}, //vincenty
        {0.0002, 0.002, 0.01, 0.2}, //thomas
        {0.002, 0.4, 15, 25}, //andoyer
        {1, 6, 15, 200} //spherical
    };

    //vincenty
    double distance = bg::strategy::distance::detail::geographic_cross_track<bg::strategy::vincenty, Spheroid, double, true>(spheroid)
            .apply(Point(lon3r, lat3r), Point(lon1r, lat1r), Point(lon2r, lat2r));
    check_result(distance, results.vincenty_bisection, results.reference, errors[0]);

    distance = bg::strategy::distance::geographic_cross_track<bg::strategy::vincenty, Spheroid, double>(spheroid)
            .apply(Point(lon3r, lat3r), Point(lon1r, lat1r), Point(lon2r, lat2r));
    check_result(distance, results.vincenty, results.reference, errors[0]);

    //thomas
    distance = bg::strategy::distance::detail::geographic_cross_track<bg::strategy::thomas, Spheroid, double, true>(spheroid)
            .apply(Point(lon3r, lat3r), Point(lon1r, lat1r), Point(lon2r, lat2r));
    check_result(distance, results.thomas_bisection, results.reference, errors[1]);

    distance = bg::strategy::distance::geographic_cross_track<bg::strategy::thomas, Spheroid, double>(spheroid)
            .apply(Point(lon3r, lat3r), Point(lon1r, lat1r), Point(lon2r, lat2r));
    check_result(distance, results.thomas, results.reference, errors[1]);

    //andoyer
    distance = bg::strategy::distance::detail::geographic_cross_track<bg::strategy::andoyer, Spheroid, double, true>(spheroid)
            .apply(Point(lon3r, lat3r), Point(lon1r, lat1r), Point(lon2r, lat2r));
    check_result(distance, results.andoyer_bisection, results.reference, errors[2]);

    distance = bg::strategy::distance::geographic_cross_track<bg::strategy::andoyer, Spheroid, double>(spheroid)
            .apply(Point(lon3r, lat3r), Point(lon1r, lat1r), Point(lon2r, lat2r));
    check_result(distance, results.andoyer, results.reference, errors[2]);

    //spherical
    distance = bg::strategy::distance::cross_track<>(bg::formula::mean_radius<double>(spheroid))
            .apply(Point(lon3r, lat3r), Point(lon1r, lat1r), Point(lon2r, lat2r));
    check_result(distance, results.spherical, results.reference, errors[3]);

}

int test_main(int, char*[])
{
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::radian> > point;

    for (size_t i = 0; i < expected_size; ++i)
    {
        test_all<point>(expected[i]);
    }

    return 0;
}
