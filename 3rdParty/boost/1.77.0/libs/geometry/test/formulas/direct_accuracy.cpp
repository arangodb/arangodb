// Boost.Geometry
// Unit Test

// Copyright (c) 2019 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/geometry/formulas/karney_direct.hpp>
#include <boost/geometry/srs/srs.hpp>

#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/Constants.hpp>
#endif // BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB

int test_main(int, char*[])
{

#ifdef BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB
    // accuracy test from https://github.com/boostorg/geometry/issues/560
    using namespace GeographicLib;

    const long double wgs84_a = 6378137.0L;
    const long double wgs84_f = 1.0L / 298.257223563L;
    const long double one_minus_f = 1.0L - wgs84_f;
    const long double wgs84_b = wgs84_a * one_minus_f;

    const boost::geometry::srs::spheroid<long double> BoostWGS84(wgs84_a, wgs84_b);

    // boost karney_direct function class with azimuth output and SeriesOrder = 6
    typedef boost::geometry::formula::karney_direct <double, true, true, false, false, 6u>
            BoostKarneyDirect_6;

    // boost karney_direct function class with azimuth output and SeriesOrder = 8
    typedef boost::geometry::formula::karney_direct <double, true, true, false, false, 8u>
            BoostKarneyDirect_8;

    // boost test BOOST_CHECK_CLOSE macro takes a percentage accuracy parameter
    const double EPSILON = std::numeric_limits<double>::epsilon();
    const double CALCULATION_TOLERANCE = 100 * EPSILON;

    const Geodesic GeographicLibWGS84(Geodesic::WGS84());

    // Loop around latitudes: 0 to 89 degrees
    for (int i=0; i < 90; ++i)
    {
        // The latitude in degrees.
        double latitude(1.0 * i);

        // Loop around longitudes: 1 to 179 degrees
        for (int j=1; j < 180; ++j)
        {
            // The longitude in degrees.
            double longitude(1.0 * j);

            // The Geodesic: distance in metres, start azimuth and finish azimuth in degrees.
            double distance_m, azimuth, azi2;
            GeographicLibWGS84.Inverse(0.0, 0.0, latitude, longitude, distance_m, azimuth, azi2);

            // The GeographicLib position and azimuth at the distance in metres
            double lat2k, lon2k, azi2k;
            GeographicLibWGS84.Direct(0.0, 0.0, azimuth, distance_m, lat2k, lon2k, azi2k);
            BOOST_CHECK_CLOSE(latitude, lat2k, 140 * CALCULATION_TOLERANCE);
            BOOST_CHECK_CLOSE(longitude, lon2k, 120 * CALCULATION_TOLERANCE);

            // The boost karney_direct order 6 position at the azimuth and distance in metres.
            boost::geometry::formula::result_direct<double> results_6
                    = BoostKarneyDirect_6::apply(0.0, 0.0, distance_m, azimuth, BoostWGS84);
            BOOST_CHECK_CLOSE(azi2, results_6.reverse_azimuth, 140 * CALCULATION_TOLERANCE);
            BOOST_CHECK_CLOSE(latitude, results_6.lat2, 220 * CALCULATION_TOLERANCE);

            /******** Test below only passes with >= 10172000 * CALCULATION_TOLERANCE !! ********/
            BOOST_CHECK_CLOSE(longitude, results_6.lon2, 10171000 * CALCULATION_TOLERANCE);
            /*****************************************************************************/

            // The boost karney_direct order 8 position at the azimuth and distance in metres.
            boost::geometry::formula::result_direct<double> results_8
                    = BoostKarneyDirect_8::apply(0.0, 0.0, distance_m, azimuth, BoostWGS84);
            BOOST_CHECK_CLOSE(azi2, results_8.reverse_azimuth, 140 * CALCULATION_TOLERANCE);
            BOOST_CHECK_CLOSE(latitude, results_8.lat2, 220 * CALCULATION_TOLERANCE);

            /******** Test below only passes with >= 10174000 * CALCULATION_TOLERANCE !! ********/
            BOOST_CHECK_CLOSE(longitude, results_8.lon2, 10173000 * CALCULATION_TOLERANCE);
            /*****************************************************************************/
        }
    }
#endif // BOOST_GEOEMTRY_TEST_WITH_GEOGRAPHICLIB

    return 0;
}

