// Boost.Geometry
// Unit Test

// Copyright (c) 2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/geometry/srs/projection.hpp>


namespace srs = bg::srs;
namespace par = bg::srs::spar;


int test_main(int, char* [])
{
    typedef par::proj_aea proj;
    typedef par::ellps_clrk80 ellps;
    typedef par::datum_ire65 datum;
    typedef par::o_proj<par::proj_tmerc> o_proj;
    typedef par::guam guam;

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_tr<par::detail::proj_traits>::pred<proj>::value),
                         PROJ, (proj));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_tr<par::detail::proj_traits>::pred<int>::value),
                         NOT_PROJ, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_tr<par::detail::ellps_traits>::pred<ellps>::value),
                         ELLPS, (ellps));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_tr<par::detail::ellps_traits>::pred<int>::value),
                         NOT_ELLPS, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_tr<par::detail::datum_traits>::pred<datum>::value),
                         DATUM, (datum));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_tr<par::detail::datum_traits>::pred<int>::value),
                         NOT_DATUM, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_t<par::o_proj>::pred<o_proj>::value),
                         O_PROJ, (o_proj));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_t<par::o_proj>::pred<int>::value),
                         NOT_O_PROJ, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param<par::guam>::pred<guam>::value),
                         GUAM, (guam));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param<par::guam>::pred<int>::value),
                         NOT_GUAM, (int));

    typedef par::parameters<proj, ellps, datum, o_proj, guam> params;
    typedef par::parameters<proj, ellps> params_e;
    typedef par::parameters<proj, datum> params_d;
    typedef par::parameters<proj> params_0;

    boost::ignore_unused<params, params_e, params_d, params_0>();

    return 0;
}
