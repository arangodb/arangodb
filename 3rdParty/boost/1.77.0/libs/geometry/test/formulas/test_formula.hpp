// Boost.Geometry
// Unit Test

// Copyright (c) 2016-2019 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_FORMULA_HPP
#define BOOST_GEOMETRY_TEST_FORMULA_HPP

#include <geometry_test_common.hpp>

#include <boost/geometry/formulas/result_inverse.hpp>
#include <boost/geometry/util/math.hpp>

void normalize_deg(double & deg)
{
    while (deg > 180.0)
        deg -= 360.0;
    while (deg <= -180.0)
        deg += 360.0;
}


#define BOOST_GEOMETRY_CHECK_CLOSE( L, R, T, M )        BOOST_TEST_TOOL_IMPL( 0, \
    ::boost::test_tools::check_is_close_t(), M, CHECK, CHECK_MSG, (L)(R)(::boost::math::fpc::percent_tolerance(T)) )


void check_one(std::string const& name, double result, double expected)
{
    std::string id = name.empty() ? "" : (name + " : ");

    double eps = std::numeric_limits<double>::epsilon();
    double abs_result = bg::math::abs(result);
    double abs_expected = bg::math::abs(expected);
    double res_max = (std::max)(abs_result, abs_expected);
    double res_min = (std::min)(abs_result, abs_expected);
    if (res_min <= eps) // including 0
    {
        bool is_close = abs_result <= 30 * eps && abs_expected <= 30 * eps;
        BOOST_CHECK_MESSAGE((is_close),
            id << std::setprecision(20) << "result {" << result
            << "} different than expected {" << expected << "}.");
    }
    else if (res_max > 100 * eps)
    {
        BOOST_GEOMETRY_CHECK_CLOSE(result, expected, 0.1,
            id << std::setprecision(20) << "result {" << result
            << "} different than expected {" << expected << "}.");
    }
    else if (res_max > 10 * eps)
    {
        BOOST_GEOMETRY_CHECK_CLOSE(result, expected, 10,
            id << std::setprecision(20) << "result {" << result
            << "} different than expected {" << expected << "}.");
    }
    else if (res_max > eps)
    {
        BOOST_GEOMETRY_CHECK_CLOSE(result, expected, 1000,
            id << std::setprecision(20) << "result {" << result
            << "} different than expected {" << expected << "}.");
    }
}

void check_one(std::string const& name,
               double result, double expected, double reference, double reference_error,
               bool normalize = false, bool check_reference_only = false)
{
    std::string id = name.empty() ? "" : (name + " : ");

    if (normalize)
    {
        normalize_deg(result);
        normalize_deg(expected);
        normalize_deg(reference);
    }

    if (! check_reference_only)
    {
        check_one(name, result, expected);
    }

    // NOTE: in some cases it probably will be necessary to normalize
    //       the differences between the result and expected result
    double ref_diff = bg::math::abs(result - reference);
    double ref_max = (std::max)(bg::math::abs(result), bg::math::abs(reference));
    bool is_ref_close = ref_diff <= reference_error || ref_diff <= reference_error * ref_max;
    BOOST_CHECK_MESSAGE((is_ref_close),
        id << std::setprecision(20) << "result {" << result << "} and reference {"
        << reference << "} not close enough.");
}

void check_one(double result, double expected, double reference, double reference_error,
               bool normalize = false, bool check_reference_only = false)
{
    check_one("", result, expected, reference, reference_error, normalize,
        check_reference_only);
}

template <typename Result, typename ExpectedResult>
void check_inverse(std::string const& name,
                   Result const& results,
                   boost::geometry::formula::result_inverse<double> const& result,
                   ExpectedResult const& expected,
                   ExpectedResult const& reference,
                   double reference_error)
{
    std::stringstream ss;
    ss << "(" << results.p1.lon << " " << results.p1.lat << ")->("
       << results.p2.lon << " " << results.p2.lat << ")";

    check_one(name + "_d  " + ss.str(),
              result.distance, expected.distance, reference.distance, reference_error);
    check_one(name + "_a  " + ss.str(),
              result.azimuth, expected.azimuth, reference.azimuth, reference_error, true);
    check_one(name + "_ra " + ss.str(),
              result.reverse_azimuth, expected.reverse_azimuth, reference.reverse_azimuth,
              reference_error, true);
    check_one(name + "_rl " + ss.str(),
              result.reduced_length, expected.reduced_length, reference.reduced_length,
              reference_error);
    check_one(name + "_gs " + ss.str(),
              result.geodesic_scale, expected.geodesic_scale, reference.geodesic_scale,
              reference_error);
}

#endif // BOOST_GEOMETRY_TEST_FORMULA_HPP
