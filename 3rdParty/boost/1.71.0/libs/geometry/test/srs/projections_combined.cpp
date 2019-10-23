// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017, 2018.
// Modifications copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#if defined(_MSC_VER)
#pragma warning( disable : 4305 ) // truncation double -> float
#endif // defined(_MSC_VER)

#define BOOST_GEOMETRY_SRS_ENABLE_STATIC_PROJECTION_HYBRID_INTERFACE

#include <geometry_test_common.hpp>

#include <boost/geometry/srs/projection.hpp>

#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/algorithms/make.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include "proj4.hpp"

namespace srs = bg::srs;

template <typename P>
bool check_expected(P const& p1, P const& p2)
{
    return bg::math::abs(bg::get<0>(p1) - bg::get<0>(p2)) <= 1.0
        && bg::math::abs(bg::get<1>(p1) - bg::get<1>(p2)) <= 1.0;
}

template <typename StaticParams, typename GeoPoint, typename CartPoint>
void test_forward(std::string const& id, GeoPoint const& geo_point, CartPoint const& cart_point,
                  StaticParams const& params, std::string const& proj4 = "")
{
    try
    {
        srs::projection<StaticParams> prj = params;

        CartPoint xy;
        prj.forward(geo_point, xy);
        
        bool ok = check_expected(xy, cart_point);

        BOOST_CHECK_MESSAGE(ok,
                " id: " << id
                << " point: " << bg::wkt(xy)
                << " different than expected: " << bg::wkt(cart_point));

        if (! proj4.empty())
        {
            srs::projection<> prj2 = srs::proj4(proj4);

            CartPoint xy2;
            prj2.forward(geo_point, xy2);

            bool eq2 = bg::equals(xy, xy2);

            BOOST_CHECK_MESSAGE(eq2,
                " id: " << id << " result of static: "
                << bg::wkt(xy) << " different than dynamic: " << bg::wkt(xy2));

#ifdef TEST_WITH_PROJ4
            pj_projection prj3(proj4);

            CartPoint xy3;
            prj3.forward(geo_point, xy3);

            bool eq3 = bg::equals(xy, xy3);

            BOOST_CHECK_MESSAGE(eq3,
                " id: " << id << " result: "
                << bg::wkt(xy) << " different than proj4: " << bg::wkt(xy3));
#endif // TEST_WITH_PROJ4
        }
    }
    catch(bg::projection_exception const& e)
    {
        std::cout << "Exception in " << id << " : " << e.code() << std::endl;
    }
    catch(...)
    {
        std::cout << "Exception (unknown) in " << id << std::endl;
    }
}

template <typename StaticParams, typename GeoPoint, typename CartPoint>
void test_forward(std::string const& id, GeoPoint const& geo_point, CartPoint const& cart_point,
                  std::string const& proj4 = "")
{
    test_forward(id, geo_point, cart_point, StaticParams(), proj4);
}

template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::geographic<bg::degree> > geo_point_type;
    typedef bg::model::point<T, 2, bg::cs::cartesian> cart;

    geo_point_type amsterdam = bg::make<geo_point_type>(4.8925, 52.3731);
    geo_point_type utrecht   = bg::make<geo_point_type>(5.1213, 52.0907);

    geo_point_type anchorage = bg::make<geo_point_type>(-149.90, 61.22);
    geo_point_type juneau    = bg::make<geo_point_type>(-134.42, 58.30);

    geo_point_type auckland   = bg::make<geo_point_type>(174.74, -36.84);
    geo_point_type wellington = bg::make<geo_point_type>(177.78, -41.29);

    geo_point_type aspen  = bg::make<geo_point_type>(-106.84, 39.19);
    geo_point_type denver = bg::make<geo_point_type>(-104.88, 39.76);

    using namespace srs::spar;

    // IGH (internally using moll/sinu)
    {
        typedef parameters<proj_igh, ellps_sphere, units_m> params_t;
        std::string sparams = "+proj=igh +ellps=sphere +units=m";
        test_forward<params_t>("igh-am", amsterdam, cart(1489299.1509520211, 5776413.4260336142), sparams);
        test_forward<params_t>("igh-ut", utrecht, cart(1498750.6627020084, 5747394.3313896423), sparams);
        test_forward<params_t>("igh-as", aspen, cart(-11708973.126426676, 4357727.1232166551), sparams);
        test_forward<params_t>("igh-de", denver, cart(-11536624.264589204, 4421108.2015589233), sparams);
        test_forward<params_t>("igh-au", auckland, cart(18658819.353676274, -4096419.1686476548), sparams);
        test_forward<params_t>("igh-we", wellington, cart(18733710.557981707, -4591140.1631184481), sparams);
        test_forward<params_t>("igh-an", anchorage, cart(-14275110.630537530, 6648284.9393376000), sparams);
        test_forward<params_t>("igh-ju", juneau, cart(-13421076.123140398, 6368936.3597440729), sparams);
    }

    // Using moll
    {
        typedef parameters<proj_ob_tran, ellps_wgs84, o_proj<proj_moll>, units_m, o_lat_p<>, o_lon_p<> > params_t;
        params_t params = params_t(proj_ob_tran(), ellps_wgs84(), o_proj<proj_moll>(), units_m(), o_lat_p<>(10), o_lon_p<>(90));
        std::string sparams = "+proj=ob_tran +ellps=WGS84 +o_proj=moll +units=m +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50";
        test_forward<params_t>("obt-m-am", amsterdam, cart(8688778.3518596273, -3348126.4518623645), params, sparams);
        test_forward<params_t>("obt-m-ut", utrecht, cart(8693109.5205437448, -3379708.1134765535), params, sparams);
        test_forward<params_t>("obt-m-as", aspen, cart(3691751.3259231807, 2371456.9674431868), params, sparams);
        test_forward<params_t>("obt-m-de", denver, cart(3764685.2104777521, 2185616.0182080171), params, sparams);
    }

    // Using sinu
    {
        // In Proj4 >= 5.0.0 ob_tran projection doesn't overwrite the ellipsoid's parameters to make sphere in underlying projection
        // So in order to use spherical sinu projection WGS84-compatible sphere has to be set manually.
        typedef parameters<proj_ob_tran, a<>, es<>, o_proj<proj_sinu>, units_m, o_lat_p<>, o_lon_p<> > params_t;
        params_t params = params_t(proj_ob_tran(), a<>(6378137), es<>(0), o_proj<proj_sinu>(), units_m(), o_lat_p<>(10), o_lon_p<>(90));
        std::string sparams = "+proj=ob_tran +a=6378137 +es=0 +o_proj=sinu +units=m +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50";
        test_forward<params_t>("obt-s-am", amsterdam, cart(9220221.4221933037, -3059652.3579233172), params, sparams);
        test_forward<params_t>("obt-s-ut", utrecht, cart(9216281.0977674071, -3089427.4415689218), params, sparams);
        test_forward<params_t>("obt-s-as", aspen, cart(4010672.3356677019, 2150730.9484995930), params, sparams);
        test_forward<params_t>("obt-s-de", denver, cart(4103945.8062708224, 1979964.9315176210), params, sparams);
    }
}

int test_main(int, char* [])
{
    test_all<double>();

    return 0;
}
