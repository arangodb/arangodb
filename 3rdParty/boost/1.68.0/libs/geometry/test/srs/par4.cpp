// Boost.Geometry
// Unit Test

// Copyright (c) 2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/srs/projection.hpp>


namespace srs = bg::srs;
namespace par = bg::srs::par4;


int test_main(int, char* [])
{
    typedef par::proj<par::aea> proj;
    typedef par::ellps<par::clrk80> ellps;
    typedef par::datum<par::ire65> datum;
    typedef par::o_proj<par::tmerc> o_proj;
    typedef par::guam guam;

    /*BOOST_MPL_ASSERT_MSG((par::detail::is_proj<proj>::value),
                         PROJ, (proj));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_proj<int>::value),
                         NOT_PROJ, (int));
    
    BOOST_MPL_ASSERT_MSG((par::detail::is_ellps<ellps>::value),
                         ELLPS, (ellps));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_ellps<int>::value),
                         NOT_ELLPS, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_datum<datum>::value),
                         DATUM, (datum));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_datum<int>::value),
                         NOT_DATUM, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_o_proj<o_proj>::value),
                         O_PROJ, (o_proj));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_o_proj<int>::value),
                         NOT_O_PROJ, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_guam<guam>::value),
                         GUAM, (guam));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_guam<int>::value),
                         NOT_GUAM, (int));*/

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_t<par::proj>::pred<proj>::value),
                         PROJ, (proj));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_t<par::proj>::pred<int>::value),
                         NOT_PROJ, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_t<par::ellps>::pred<ellps>::value),
                         ELLPS, (ellps));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_t<par::ellps>::pred<int>::value),
                         NOT_ELLPS, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_t<par::datum>::pred<datum>::value),
                         DATUM, (datum));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_t<par::datum>::pred<int>::value),
                         NOT_DATUM, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param_t<par::o_proj>::pred<o_proj>::value),
                         O_PROJ, (o_proj));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param_t<par::o_proj>::pred<int>::value),
                         NOT_O_PROJ, (int));

    BOOST_MPL_ASSERT_MSG((par::detail::is_param<par::guam>::pred<guam>::value),
                         GUAM, (guam));
    BOOST_MPL_ASSERT_MSG((!par::detail::is_param<par::guam>::pred<int>::value),
                         NOT_GUAM, (int));

    typedef srs::static_proj4<proj, ellps, datum, o_proj, guam> params;
    typedef srs::static_proj4<proj, ellps> params_e;
    typedef srs::static_proj4<proj, datum> params_d;
    typedef srs::static_proj4<proj> params_0;

    BOOST_MPL_ASSERT_MSG((boost::is_same<par::detail::pick_proj_tag<params>::type, par::aea>::value),
                         PICK_PROJ, (params));
    BOOST_MPL_ASSERT_MSG((boost::is_same<par::detail::pick_ellps<params>::type::type, par::clrk80>::value),
                         PICK_ELLPS, (params));
    BOOST_MPL_ASSERT_MSG((boost::is_same<par::detail::pick_o_proj_tag<params>::type, par::tmerc>::value),
                         PICK_O_PROJ, (params));
    BOOST_MPL_ASSERT_MSG((boost::is_same<par::detail::pick_ellps<params_e>::type::type, par::clrk80>::value),
                         PICK_ELLPS_E, (params_e));
    BOOST_MPL_ASSERT_MSG((boost::is_same<par::detail::pick_ellps<params_d>::type::type, par::mod_airy>::value),
                         PICK_ELLPS_D, (params_d));
    //default ellps WGS84
    BOOST_MPL_ASSERT_MSG((boost::is_same<par::detail::pick_ellps<params_0>::type::type, par::WGS84>::value),
                         PICK_NO_ELLPS, (params_0));

    return 0;
}
