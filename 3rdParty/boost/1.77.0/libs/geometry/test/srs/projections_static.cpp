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

#include <geometry_test_common.hpp>

#include <boost/geometry/srs/projection.hpp>

#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/algorithms/make.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>


template <template <typename, typename> class Projection, typename GeoPoint>
void test_forward(GeoPoint const& geo_point1, GeoPoint const& geo_point2,
        std::string const& sparams, int deviation = 1)
{
    typedef typename bg::coordinate_type<GeoPoint>::type coordinate_type;
    typedef bg::model::d2::point_xy<coordinate_type> cartesian_point_type;
    typedef bg::projections::parameters<double> parameters_type;
    typedef bg::projections::detail::static_wrapper_f
        <
            Projection<coordinate_type, parameters_type>,
            parameters_type
        > projection_type;

    try
    {
        bg::srs::detail::proj4_parameters params(sparams);
        parameters_type par = bg::projections::detail::pj_init<double>(params);

        projection_type prj(params, par);

        cartesian_point_type xy1, xy2;
        prj.forward(geo_point1, xy1);
        prj.forward(geo_point2, xy2);

        // Calculate distances in KM
        int const distance_expected = static_cast<int>(bg::distance(geo_point1, geo_point2) / 1000.0);
        int const distance_found = static_cast<int>(bg::distance(xy1, xy2) / 1000.0);

        int const difference = std::abs(distance_expected - distance_found);
        BOOST_CHECK_MESSAGE(difference <= 1 || difference == deviation,
                " projection: " << projection_type::get_name()
                << " distance found: " << distance_found
                << " expected: " << distance_expected);

// For debug:
//        std::cout << projection_type::get_name() << " " << distance_expected
//            << " " << distance_found
//            << " " << (difference > 1 && difference != deviation ? " *** WRONG ***" : "")
//            << " " << difference
//            << std::endl;
    }
    catch(bg::projection_exception const& e)
    {
        std::cout << "Exception in " << projection_type::get_name() << " : " << e.what() << std::endl;
    }
    catch(...)
    {
        std::cout << "Exception (unknown) in " << projection_type::get_name() << std::endl;
    }
}

template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::geographic<bg::degree> > geo_point_type;

    geo_point_type amsterdam = bg::make<geo_point_type>(4.8925, 52.3731);
    geo_point_type utrecht   = bg::make<geo_point_type>(5.1213, 52.0907);

    // IMPORTANT: Compatible model has to be passed in order to assure correct initialization

    test_forward<bg::projections::aea_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lat_1=55 +lat_2=65");
    test_forward<bg::projections::aeqd_e>(amsterdam, utrecht, "+ellps=WGS84 +units=m");
    test_forward<bg::projections::aeqd_s>(amsterdam, utrecht, "+ellps=sphere +units=m");

    test_forward<bg::projections::airy_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 4);
    test_forward<bg::projections::aitoff_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::apian_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lon_0=11d32'00E");
    test_forward<bg::projections::august_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 14);

    test_forward<bg::projections::bacon_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lon_0=11d32'00E", 5);
    test_forward<bg::projections::bipc_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 7);
    test_forward<bg::projections::boggs_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lon_0=11d32'00E", 2);

    test_forward<bg::projections::bonne_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lat_1=50");
    test_forward<bg::projections::bonne_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=50", 33);

    test_forward<bg::projections::cass_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m");
    test_forward<bg::projections::cass_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::cc_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 52);

    test_forward<bg::projections::cea_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lon_0=11d32'00E", 4);
    test_forward<bg::projections::cea_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lon_0=11d32'00E", 4);

    test_forward<bg::projections::chamb_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=52 +lon_1=5 +lat_2=30 +lon_2=80 +lat_3=20 +lon_3=-50", 2);
    test_forward<bg::projections::collg_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 5);
    test_forward<bg::projections::crast_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::denoy_spheroid>(amsterdam, utrecht,  "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::eck1_spheroid>(amsterdam, utrecht,  "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::eck2_spheroid>(amsterdam, utrecht,  "+ellps=sphere +units=m");
    test_forward<bg::projections::eck3_spheroid>(amsterdam, utrecht,  "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::eck4_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::eck5_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);

    test_forward<bg::projections::eck6_spheroid>(amsterdam, utrecht,  "+ellps=sphere +units=m");

    test_forward<bg::projections::eqc_spheroid>(amsterdam, utrecht,  "+ellps=sphere +units=m", 5);
    test_forward<bg::projections::eqdc_ellipsoid>(amsterdam, utrecht,  "+ellps=WGS84 +units=m +lat_1=60 +lat_2=0");
    test_forward<bg::projections::etmerc_ellipsoid>(amsterdam, utrecht,  "+ellps=WGS84 +units=m");
    test_forward<bg::projections::euler_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=60 +lat_2=0");

    test_forward<bg::projections::fahey_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 5);
    test_forward<bg::projections::fouc_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 6);
    test_forward<bg::projections::fouc_s_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 4);
    test_forward<bg::projections::gall_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::geocent_other>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 5);
    test_forward<bg::projections::geos_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +h=40000000", 13);
    test_forward<bg::projections::gins8_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 7);

    test_forward<bg::projections::gn_sinu_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +m=0.5 +n=1.785");

    test_forward<bg::projections::gnom_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 50);
    test_forward<bg::projections::goode_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::gstmerc_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::hammer_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::hatano_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);

    test_forward<bg::projections::healpix_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 6);
    test_forward<bg::projections::healpix_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 6);
    test_forward<bg::projections::rhealpix_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 6);
    test_forward<bg::projections::rhealpix_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 6);

    test_forward<bg::projections::imw_p_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lat_1=20n +lat_2=60n +lon_1=5");
    test_forward<bg::projections::isea_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::kav5_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::kav7_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::krovak_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m");
    test_forward<bg::projections::laea_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::lagrng_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +W=1", 8);
    test_forward<bg::projections::larr_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 13);
    test_forward<bg::projections::lask_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 5);
    test_forward<bg::projections::lcc_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lat_1=20n +lat_2=60n", 2);
    test_forward<bg::projections::lcca_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lat_0=30n +lat_1=55n +lat_2=60n", 2);
    test_forward<bg::projections::leac_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 8);
    test_forward<bg::projections::loxim_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 3);
    test_forward<bg::projections::lsat_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lsat=1 +path=1", 3);
    test_forward<bg::projections::mbt_fps_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::mbt_s_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::mbtfpp_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::mbtfpq_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);

    test_forward<bg::projections::mbtfps_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");

    test_forward<bg::projections::merc_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 22);
    test_forward<bg::projections::merc_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 22);

    test_forward<bg::projections::mil_os_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m");
    test_forward<bg::projections::mill_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 14);
    test_forward<bg::projections::moll_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::murd1_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n");
    test_forward<bg::projections::murd2_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n");
    test_forward<bg::projections::murd3_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n");
    test_forward<bg::projections::natearth_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::nell_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 4);
    test_forward<bg::projections::nell_h_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 3);
    test_forward<bg::projections::nicol_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");

    test_forward<bg::projections::oea_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n +lon_1=1e +lon_2=30e +m=1 +n=1", 4);
    test_forward<bg::projections::omerc_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lat_1=20n +lat_2=60n  +lon_1=1e +lon_2=30e");
    test_forward<bg::projections::ortel_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::ortho_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 9);
    test_forward<bg::projections::pconic_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n +lon_0=10E");
    test_forward<bg::projections::qsc_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 10);
    test_forward<bg::projections::poly_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::putp1_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::putp2_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::putp3_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 6);
    test_forward<bg::projections::putp3p_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 5);
    test_forward<bg::projections::putp4p_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::putp5_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::putp5p_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 3);
    test_forward<bg::projections::putp6_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::putp6p_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::qua_aut_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::robin_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::rouss_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 8);
    test_forward<bg::projections::rpoly_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");

    test_forward<bg::projections::sinu_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m");
    test_forward<bg::projections::sinu_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");

    test_forward<bg::projections::somerc_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 22);
    test_forward<bg::projections::stere_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_ts=50n", 8);
    test_forward<bg::projections::sterea_ellipsoid>(amsterdam, utrecht, "+lat_0=52.15616055555555 +lon_0=5.38763888888889 +k=0.9999079 +x_0=155000 +y_0=463000 +ellps=bessel +units=m");
    test_forward<bg::projections::tcc_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::tcea_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::tissot_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n", 2);

    test_forward<bg::projections::tmerc_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::tmerc_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m");

    test_forward<bg::projections::tpeqd_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n  +lon_1=0 +lon_2=30e");
    test_forward<bg::projections::tpers_spheroid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +tilt=50 +azi=20 +h=40000000", 14);

    // Elliptical usage required
    // TODO: if spherical UPS is not supported then ups_spheroid should be removed
    //test_forward<bg::projections::ups_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 3);
    test_forward<bg::projections::ups_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m", 3);

    test_forward<bg::projections::urm5_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +n=.3 +q=.3 +alpha=10", 4);
    test_forward<bg::projections::urmfps_spheroid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +n=0.50", 4);

    test_forward<bg::projections::utm_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +lon_0=11d32'00E");

    test_forward<bg::projections::vandg_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 13);
    test_forward<bg::projections::vandg2_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 13);
    test_forward<bg::projections::vandg3_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 13);
    test_forward<bg::projections::vandg4_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::vitk1_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m +lat_1=20n +lat_2=60n");
    test_forward<bg::projections::wag1_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::wag2_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::wag3_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 3);
    test_forward<bg::projections::wag4_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::wag5_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::wag6_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");
    test_forward<bg::projections::wag7_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 2);
    test_forward<bg::projections::weren_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 4);
    test_forward<bg::projections::wink1_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 3);
    test_forward<bg::projections::wink2_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m", 4);
    test_forward<bg::projections::wintri_spheroid>(amsterdam, utrecht, "+ellps=sphere +units=m");

    //    We SKIP ob_tran because it internally requires the factory and is, in that sense, not a static test
    //    test_forward<bg::projections::ob_tran_oblique>(amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50");
    //    test_forward<bg::projections::ob_tran_transverse>(amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50");

    // TODO: wrong projections or parameters or input points
//    test_forward<bg::projections::ocea_spheroid>(auckland, wellington, "+ellps=sphere +units=m +lat_1=20s +lat_2=60s  +lon_1=165e +lon_2=175e"); => distance is very large
//    test_forward<bg::projections::nsper_spheroid>(amsterdam, utrecht, "+ellps=WGS84 +units=m +a=10 +h=40000000"); => distance is 0
//    test_forward<bg::projections::lee_os_ellipsoid>(amsterdam, utrecht, "+ellps=WGS84 +units=m"); => distance is 407


    // Alaska
    {
        geo_point_type anchorage = bg::make<geo_point_type>(-149.90, 61.22);
        geo_point_type juneau    = bg::make<geo_point_type>(-134.42, 58.30);
        test_forward<bg::projections::alsk_ellipsoid>(anchorage, juneau, "+ellps=WGS84 +units=m +lon_0=-150W", 1);
    }
    // New Zealand
    {
        geo_point_type auckland   = bg::make<geo_point_type>(174.74, -36.84);
        geo_point_type wellington = bg::make<geo_point_type>(177.78, -41.29);
        test_forward<bg::projections::nzmg_ellipsoid>(auckland, wellington, "+ellps=WGS84 +units=m", 0);
    }

    // US
    {
        geo_point_type aspen  = bg::make<geo_point_type>(-106.84, 39.19);
        geo_point_type denver = bg::make<geo_point_type>(-104.88, 39.76);
        // TODO: test_forward<bg::projections::gs48_ellipsoid>(aspen, denver, "+ellps=WGS84 +units=m +lon1=-48");=> distance is > 1000
        test_forward<bg::projections::gs50_ellipsoid>(aspen, denver, "+ellps=WGS84 +units=m +lon1=-50", 2);
    }

}

int test_main(int, char* [])
{
    test_all<double>();

    return 0;
}
