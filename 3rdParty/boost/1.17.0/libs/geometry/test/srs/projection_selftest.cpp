// Boost.Geometry
// Unit Test

// Copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <iostream>

#include <boost/geometry/srs/projection.hpp>

#include "projection_selftest_cases.hpp"
#include "proj4.hpp"


void test_projection(std::string const& id, std::string const& parameters,
                     const LL * fwd_in, const XY * fwd_expected,
                     const XY * inv_in, const LL * inv_expected)
{
    bg::srs::projection<> prj = bg::srs::proj4(parameters);

#ifdef TEST_WITH_PROJ4
    pj_projection pj_par(parameters);
#endif

    for (std::size_t i = 0 ; i < 4 ; ++i)
    {
        if (bg::get<0>(fwd_expected[i]) == HUGE_VAL)
            break;

        {
            XY fwd_out;
            prj.forward(fwd_in[i], fwd_out);

            bool fwd_eq = bg::math::abs(bg::get<0>(fwd_out) - bg::get<0>(fwd_expected[i])) < 1e-7
                       && bg::math::abs(bg::get<1>(fwd_out) - bg::get<1>(fwd_expected[i])) < 1e-7;

            BOOST_CHECK_MESSAGE((fwd_eq),
                                std::setprecision(16) << "Result of " << id << " forward projection {"
                                << bg::wkt(fwd_out) << "} different than expected {"
                                << bg::wkt(fwd_expected[i]) << "}");

#ifdef TEST_WITH_PROJ4
            {
                XY pj_xy;
                pj_par.forward(fwd_in[i], pj_xy);
                double d1 = bg::math::abs(bg::get<0>(fwd_out) - bg::get<0>(pj_xy));
                double d2 = bg::math::abs(bg::get<1>(fwd_out) - bg::get<1>(pj_xy));
                double d = (std::max)(d1, d2);
                bool same_as_pj = d < 1e-15;
                BOOST_CHECK_MESSAGE((same_as_pj),
                                    std::setprecision(16) << "Result of " << id << " forward projection {"
                                    << bg::wkt(fwd_out) << "} different than Proj4 {"
                                    << bg::wkt(pj_xy) << "} by " << d);
            }
#endif
        }

        if (bg::get<0>(inv_expected[i]) == HUGE_VAL)
            break;

        {
            LL inv_out;        
            prj.inverse(inv_in[i], inv_out);

            bool inv_eq = bg::math::abs(bg::get<0>(inv_out) - bg::get<0>(inv_expected[i])) < 1e-7
                       && bg::math::abs(bg::get<1>(inv_out) - bg::get<1>(inv_expected[i])) < 1e-7;

            BOOST_CHECK_MESSAGE((inv_eq),
                                std::setprecision(16) << "Result of " << id << " inverse projection {"
                                << bg::wkt(inv_out) << "} different than expected {"
                                << bg::wkt(inv_expected[i]) << "}");

#ifdef TEST_WITH_PROJ4
            {
                LL pj_ll;
                pj_par.inverse(inv_in[i], pj_ll);
                double d1 = bg::math::abs(bg::get<0>(inv_out) - bg::get<0>(pj_ll));
                double d2 = bg::math::abs(bg::get<1>(inv_out) - bg::get<1>(pj_ll));
                double d = (std::max)(d1, d2);
                bool same_as_pj = d < 1e-15;
                BOOST_CHECK_MESSAGE((same_as_pj),
                                    std::setprecision(16) << "Result of " << id << " inverse projection {"
                                    << bg::wkt(inv_out) << "} different than Proj4 {"
                                    << bg::wkt(pj_ll) << "} by " << d);
            }
#endif
        }
    }
}

void test_projections(const projection_case * cases, std::size_t n)
{
    for (std::size_t i = 0 ; i < n ; ++i)
    {
        projection_case const& pcas = cases[i];

        test_projection(pcas.id, pcas.args,
                        pcas.fwd_in, pcas.fwd_expect,
                        pcas.inv_in, pcas.inv_expect);
    }
}

int test_main(int, char*[])
{  
    test_projections(projection_cases, sizeof(projection_cases)/sizeof(projection_case));

    return 0;
}
