// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2021 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Copyright (c) 2018 Adeel Ahmad, Islamabad, Pakistan.

// Contributed and/or modified by Adeel Ahmad, as part of Google Summer of Code 2018 program

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <sstream>

#include "test_formula.hpp"
#include "inverse_cases.hpp"
#include "inverse_cases_antipodal.hpp"
#include "inverse_cases_small_angles.hpp"

#include <boost/geometry/formulas/karney_inverse.hpp>

#include <boost/geometry/srs/spheroid.hpp>

#include <boost/geometry/util/math.hpp>

void test_all(expected_results const& results)
{
    double lon1d = results.p1.lon * bg::math::d2r<double>();
    double lat1d = results.p1.lat * bg::math::d2r<double>();
    double lon2d = results.p2.lon * bg::math::d2r<double>();
    double lat2d = results.p2.lat * bg::math::d2r<double>();

    // WGS84
    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);

    bg::formula::result_inverse<double> result_k;

    typedef bg::formula::karney_inverse<double, true, true, true, true, true> ka_t;
    result_k = ka_t::apply(lon1d, lat1d, lon2d, lat2d, spheroid);
    result_k.azimuth *= bg::math::r2d<double>();
    result_k.reverse_azimuth *= bg::math::r2d<double>();
    check_inverse("karney", results, result_k, results.vincenty, results.reference, 0.0000001);
}

template <typename ExpectedResults>
void test_karney(ExpectedResults const& results)
{
    double lon1d = results.p1.lon * bg::math::d2r<double>();
    double lat1d = results.p1.lat * bg::math::d2r<double>();
    double lon2d = results.p2.lon * bg::math::d2r<double>();
    double lat2d = results.p2.lat * bg::math::d2r<double>();


    // WGS84
    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);

    bg::formula::result_inverse<double> result;

    typedef bg::formula::karney_inverse<double, true, true, true, true, true> ka_t;
    result = ka_t::apply(lon1d, lat1d, lon2d, lat2d, spheroid);
    result.azimuth *= bg::math::r2d<double>();
    result.reverse_azimuth *= bg::math::r2d<double>();
    check_inverse("karney", results, result, results.karney, results.karney, 0.0000001);
}

int test_main(int, char*[])
{
    for (size_t i = 0; i < expected_size; ++i)
    {
        test_all(expected[i]);
    }

    for (size_t i = 0; i < expected_size_antipodal; ++i)
    {
        test_karney(expected_antipodal[i]);
    }

    for (size_t i = 0; i < expected_size_small_angles; ++i)
    {
        test_karney(expected_small_angles[i]);
    }

    return 0;
}
