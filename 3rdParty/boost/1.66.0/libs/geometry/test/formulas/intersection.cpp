// Boost.Geometry
// Unit Test

// Copyright (c) 2016 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_formula.hpp"
#include "intersection_cases.hpp"

#include <boost/geometry/formulas/andoyer_inverse.hpp>
#include <boost/geometry/formulas/geographic.hpp>
#include <boost/geometry/formulas/gnomonic_intersection.hpp>
#include <boost/geometry/formulas/sjoberg_intersection.hpp>
#include <boost/geometry/formulas/thomas_direct.hpp>
#include <boost/geometry/formulas/thomas_inverse.hpp>
#include <boost/geometry/formulas/vincenty_direct.hpp>
#include <boost/geometry/formulas/vincenty_inverse.hpp>

void check_result(expected_result const& result, expected_result const& expected,
                  expected_result const& reference, double reference_error,
                  bool check_reference_only)
{
    //BOOST_CHECK_MESSAGE((false), "(" << result.lon << " " << result.lat << ") vs (" << expected.lon << " " << expected.lat << ")");
    check_one(result.lon, expected.lon, reference.lon, reference_error, false, check_reference_only);
    check_one(result.lat, expected.lat, reference.lat, reference_error, false, check_reference_only);
}

void test_formulas(expected_results const& results, bool check_reference_only)
{
    // reference result
    if (results.sjoberg_vincenty.lon == ND)
    {
        return;
    }

    double const d2r = bg::math::d2r<double>();
    double const r2d = bg::math::r2d<double>();

    double lona1r = results.p1.lon * d2r;
    double lata1r = results.p1.lat * d2r;
    double lona2r = results.p2.lon * d2r;
    double lata2r = results.p2.lat * d2r;
    double lonb1r = results.q1.lon * d2r;
    double latb1r = results.q1.lat * d2r;
    double lonb2r = results.q2.lon * d2r;
    double latb2r = results.q2.lat * d2r;

    expected_result result;

    // WGS84
    bg::srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);

    if (results.gnomonic_vincenty.lon != ND)
    {
        bg::formula::gnomonic_intersection<double, bg::formula::vincenty_inverse, bg::formula::vincenty_direct>
            ::apply(lona1r, lata1r, lona2r, lata2r, lonb1r, latb1r, lonb2r, latb2r, result.lon, result.lat, spheroid);
        result.lon *= r2d;
        result.lat *= r2d;
        check_result(result, results.gnomonic_vincenty, results.sjoberg_vincenty, 0.00000001, check_reference_only);
    }

    if (results.gnomonic_thomas.lon != ND)
    {
        bg::formula::gnomonic_intersection<double, bg::formula::thomas_inverse, bg::formula::thomas_direct>
            ::apply(lona1r, lata1r, lona2r, lata2r, lonb1r, latb1r, lonb2r, latb2r, result.lon, result.lat, spheroid);
        result.lon *= r2d;
        result.lat *= r2d;
        check_result(result, results.gnomonic_thomas, results.sjoberg_vincenty, 0.0000001, check_reference_only);
    }

    if (results.sjoberg_vincenty.lon != ND)
    {
        bg::formula::sjoberg_intersection<double, bg::formula::vincenty_inverse, 4>
            ::apply(lona1r, lata1r, lona2r, lata2r, lonb1r, latb1r, lonb2r, latb2r, result.lon, result.lat, spheroid);
        result.lon *= r2d;
        result.lat *= r2d;
        check_result(result, results.sjoberg_vincenty, results.sjoberg_vincenty, 0.00000001, check_reference_only);
    }

    if (results.sjoberg_thomas.lon != ND)
    {
        bg::formula::sjoberg_intersection<double, bg::formula::thomas_inverse, 2>
            ::apply(lona1r, lata1r, lona2r, lata2r, lonb1r, latb1r, lonb2r, latb2r, result.lon, result.lat, spheroid);
        result.lon *= r2d;
        result.lat *= r2d;
        check_result(result, results.sjoberg_thomas, results.sjoberg_vincenty, 0.0000001, check_reference_only);
    }

    if (results.sjoberg_andoyer.lon != ND)
    {
        bg::formula::sjoberg_intersection<double, bg::formula::andoyer_inverse, 1>
            ::apply(lona1r, lata1r, lona2r, lata2r, lonb1r, latb1r, lonb2r, latb2r, result.lon, result.lat, spheroid);
        result.lon *= r2d;
        result.lat *= r2d;
        check_result(result, results.sjoberg_andoyer, results.sjoberg_vincenty, 0.0001, check_reference_only);
    }

    if (results.great_elliptic.lon != ND)
    {
        typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > point_geo;
        typedef bg::model::point<double, 3, bg::cs::cartesian> point_3d;
        point_geo a1(results.p1.lon, results.p1.lat);
        point_geo a2(results.p2.lon, results.p2.lat);
        point_geo b1(results.q1.lon, results.q1.lat);
        point_geo b2(results.q2.lon, results.q2.lat);
        point_3d a1v = bg::formula::geo_to_cart3d<point_3d>(a1, spheroid);
        point_3d a2v = bg::formula::geo_to_cart3d<point_3d>(a2, spheroid);
        point_3d b1v = bg::formula::geo_to_cart3d<point_3d>(b1, spheroid);
        point_3d b2v = bg::formula::geo_to_cart3d<point_3d>(b2, spheroid);
        point_3d resv(0, 0, 0);
        point_geo res(0, 0);
        bg::formula::great_elliptic_intersection(a1v, a2v, b1v, b2v, resv, spheroid);
        res = bg::formula::cart3d_to_geo<point_geo>(resv, spheroid);
        result.lon = bg::get<0>(res);
        result.lat = bg::get<1>(res);
        check_result(result, results.great_elliptic, results.sjoberg_vincenty, 0.01, check_reference_only);
    }
}

void test_4_input_combinations(expected_results const& results, bool check_reference_only)
{
    test_formulas(results, check_reference_only);

#ifdef BOOST_GEOMETRY_TEST_GEO_INTERSECTION_TEST_SIMILAR
    {
        expected_results results_alt = results;
        std::swap(results_alt.p1, results_alt.p2);
        test_formulas(results_alt, true);
    }
    {
        expected_results results_alt = results;
        std::swap(results_alt.q1, results_alt.q2);
        test_formulas(results_alt, true);
    }
    {
        expected_results results_alt = results;
        std::swap(results_alt.p1, results_alt.p2);
        std::swap(results_alt.q1, results_alt.q2);
        test_formulas(results_alt, true);
    }
#endif
}

void test_all(expected_results const& results)
{
    test_4_input_combinations(results, false);

#ifdef BOOST_GEOMETRY_TEST_GEO_INTERSECTION_TEST_SIMILAR
    expected_results results_alt = results;
    results_alt.p1.lat *= -1;
    results_alt.p2.lat *= -1;
    results_alt.q1.lat *= -1;
    results_alt.q2.lat *= -1;
    results_alt.gnomonic_vincenty.lat *= -1;
    results_alt.gnomonic_thomas.lat *= -1;
    results_alt.sjoberg_vincenty.lat *= -1;
    results_alt.sjoberg_thomas.lat *= -1;
    results_alt.sjoberg_andoyer.lat *= -1;
    results_alt.great_elliptic.lat *= -1;
    test_4_input_combinations(results_alt, true);
#endif
}

int test_main(int, char*[])
{
    for (size_t i = 0; i < expected_size; ++i)
    {
        test_all(expected[i]);
    }

    return 0;
}
