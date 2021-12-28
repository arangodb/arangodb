// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2019 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Copyright (c) 2018 Adeel Ahmad, Islamabad, Pakistan.

// Contributed and/or modified by Adeel Ahmad, as part of Google Summer of Code 2018 program

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <sstream>

#include "test_formula.hpp"
#include "inverse_cases.hpp"

#include <boost/geometry/formulas/vincenty_inverse.hpp>
#include <boost/geometry/formulas/thomas_inverse.hpp>
#include <boost/geometry/formulas/andoyer_inverse.hpp>

#include <boost/geometry/srs/spheroid.hpp>

void test_all(expected_results const& results)
{
    double const d2r = bg::math::d2r<double>();
    double const r2d = bg::math::r2d<double>();

    double lon1r = results.p1.lon * d2r;
    double lat1r = results.p1.lat * d2r;
    double lon2r = results.p2.lon * d2r;
    double lat2r = results.p2.lat * d2r;

    // WGS84
    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);

    bg::formula::result_inverse<double> result_v, result_t, result_a;

    typedef bg::formula::vincenty_inverse<double, true, true, true, true, true> vi_t;
    result_v = vi_t::apply(lon1r, lat1r, lon2r, lat2r, spheroid);
    result_v.azimuth *= r2d;
    result_v.reverse_azimuth *= r2d;
    check_inverse("vincenty", results, result_v, results.vincenty, results.reference, 0.0000001);

    typedef bg::formula::thomas_inverse<double, true, true, true, true, true> th_t;
    result_t = th_t::apply(lon1r, lat1r, lon2r, lat2r, spheroid);
    result_t.azimuth *= r2d;
    result_t.reverse_azimuth *= r2d;
    check_inverse("thomas", results, result_t, results.thomas, results.reference, 0.00001);

    typedef bg::formula::andoyer_inverse<double, true, true, true, true, true> an_t;
    result_a = an_t::apply(lon1r, lat1r, lon2r, lat2r, spheroid);
    result_a.azimuth *= r2d;
    result_a.reverse_azimuth *= r2d;
    check_inverse("andoyer", results, result_a, results.andoyer, results.reference, 0.001);
}

int test_main(int, char*[])
{
    for (size_t i = 0; i < expected_size; ++i)
    {
        test_all(expected[i]);
    }

    return 0;
}
