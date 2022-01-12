// Boost.Geometry
// Unit Test

// Copyright (c) 2018-2021, Oracle and/or its affiliates.
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

    BOOST_GEOMETRY_STATIC_ASSERT(
        (par::detail::is_param_tr<par::detail::proj_traits>::pred<proj>::value),
        "proj", proj);
    BOOST_GEOMETRY_STATIC_ASSERT(
        (!par::detail::is_param_tr<par::detail::proj_traits>::pred<int>::value),
        "not proj", int);

    BOOST_GEOMETRY_STATIC_ASSERT(
        (par::detail::is_param_tr<par::detail::ellps_traits>::pred<ellps>::value),
        "ellps", ellps);
    BOOST_GEOMETRY_STATIC_ASSERT(
        (!par::detail::is_param_tr<par::detail::ellps_traits>::pred<int>::value),
        "not ellps", int);

    BOOST_GEOMETRY_STATIC_ASSERT(
        (par::detail::is_param_tr<par::detail::datum_traits>::pred<datum>::value),
        "datum", datum);
    BOOST_GEOMETRY_STATIC_ASSERT(
        (!par::detail::is_param_tr<par::detail::datum_traits>::pred<int>::value),
        "not datum", int);

    BOOST_GEOMETRY_STATIC_ASSERT(
        (par::detail::is_param_t<par::o_proj>::pred<o_proj>::value),
        "o_proj", o_proj);
    BOOST_GEOMETRY_STATIC_ASSERT(
        (!par::detail::is_param_t<par::o_proj>::pred<int>::value),
        "not o_proj", int);

    BOOST_GEOMETRY_STATIC_ASSERT(
        (par::detail::is_param<par::guam>::pred<guam>::value),
        "guam", guam);
    BOOST_GEOMETRY_STATIC_ASSERT(
        (!par::detail::is_param<par::guam>::pred<int>::value),
        "not guam", int);

    typedef par::parameters<proj, ellps, datum, o_proj, guam> params;
    typedef par::parameters<proj, ellps> params_e;
    typedef par::parameters<proj, datum> params_d;
    typedef par::parameters<proj> params_0;

    boost::ignore_unused<params, params_e, params_d, params_0>();

    return 0;
}
