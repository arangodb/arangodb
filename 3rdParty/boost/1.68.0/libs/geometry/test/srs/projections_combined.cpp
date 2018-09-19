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


namespace srs = bg::srs;
namespace par = bg::srs::par4;

template <typename StaticProj, typename GeoPoint>
void test_forward(std::string const& id, GeoPoint const& geo_point1, GeoPoint const& geo_point2,
        std::string const& parameters, int deviation = 1)
{
    typedef typename bg::coordinate_type<GeoPoint>::type coordinate_type;
    typedef bg::model::d2::point_xy<coordinate_type> cartesian_point_type;
    typedef srs::projection<StaticProj> projection_type;

    try
    {
        // hybrid interface disabled by default
        // static_proj4 default ctor, dynamic parameters passed
        projection_type prj = srs::proj4(parameters);

        cartesian_point_type xy1, xy2;
        prj.forward(geo_point1, xy1);
        prj.forward(geo_point2, xy2);

        // Calculate distances in KM
        int const distance_expected = static_cast<int>(bg::distance(geo_point1, geo_point2) / 1000.0);
        int const distance_found = static_cast<int>(bg::distance(xy1, xy2) / 1000.0);

        int const difference = std::abs(distance_expected - distance_found);
        BOOST_CHECK_MESSAGE(difference <= 1 || difference == deviation,
                " id: " << id
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
        std::cout << "Exception in " << id << " : " << e.code() << std::endl;
    }
    catch(...)
    {
        std::cout << "Exception (unknown) in " << id << std::endl;
    }
}

template <typename Proj, typename Model, typename GeoPoint>
void test_forward(std::string const& id, GeoPoint const& geo_point1, GeoPoint const& geo_point2,
        std::string const& parameters, int deviation = 1)
{
    typedef srs::static_proj4<par::proj<Proj>, par::ellps<Model> > static_proj4;

    test_forward<static_proj4>(id, geo_point1, geo_point2, parameters, deviation);
}

template <typename Proj, typename Model, typename OProj, typename GeoPoint>
void test_forward(std::string const& id, GeoPoint const& geo_point1, GeoPoint const& geo_point2,
        std::string const& parameters, int deviation = 1)
{
    typedef srs::static_proj4<par::proj<Proj>, par::ellps<Model>, par::o_proj<OProj> > static_proj4;

    test_forward<static_proj4>(id, geo_point1, geo_point2, parameters, deviation);
}

template <typename T>
void test_all()
{
    typedef bg::model::point<T, 2, bg::cs::geographic<bg::degree> > geo_point_type;

    geo_point_type amsterdam = bg::make<geo_point_type>(4.8925, 52.3731);
    geo_point_type utrecht   = bg::make<geo_point_type>(5.1213, 52.0907);

    geo_point_type anchorage = bg::make<geo_point_type>(-149.90, 61.22);
    geo_point_type juneau    = bg::make<geo_point_type>(-134.42, 58.30);

    geo_point_type auckland   = bg::make<geo_point_type>(174.74, -36.84);
    geo_point_type wellington = bg::make<geo_point_type>(177.78, -41.29);

    geo_point_type aspen  = bg::make<geo_point_type>(-106.84, 39.19);
    geo_point_type denver = bg::make<geo_point_type>(-104.88, 39.76);

    // IGH (internally using moll/sinu)
    test_forward<par::igh, par::sphere>("igh-au", amsterdam, utrecht, "+ellps=sphere +units=m", 5);
    test_forward<par::igh, par::sphere>("igh-ad", aspen, denver, "+ellps=sphere +units=m", 3);
    test_forward<par::igh, par::sphere>("igh-aw", auckland, wellington, "+ellps=sphere +units=m", 152);
    test_forward<par::igh, par::sphere>("igh-aj", anchorage, juneau, "+ellps=sphere +units=m", 28);

    // Using moll
    //test_forward<par::ob_tran_oblique, par::WGS84>("obto-au", amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 4);
    //test_forward<par::ob_tran_transverse, par::WGS84>("obtt-au", amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 5);
    //test_forward<par::ob_tran, par::WGS84, par::moll>("obt-au", amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 4);
    test_forward<par::ob_tran, par::WGS84, par::moll>("obt-au", amsterdam, utrecht, "+units=m +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 4);
    //test_forward<par::ob_tran_oblique, par::WGS84>("obto-ad", aspen, denver, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 19);
    //test_forward<par::ob_tran_transverse, par::WGS84>("obtt-ad", aspen, denver, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 19);
    //test_forward<par::ob_tran, par::WGS84, par::moll>("obt-ad", aspen, denver, "+ellps=WGS84 +units=m +o_proj=moll +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 19);
    test_forward<par::ob_tran, par::WGS84, par::moll>("obt-ad", aspen, denver, "+units=m +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 19);

    // Using sinu
    //test_forward<par::ob_tran_oblique, par::WGS84>("obto-au", amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=sinu +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 5);
    //test_forward<par::ob_tran_transverse, par::WGS84>("obtt-au", amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=sinu +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 4);
    //test_forward<par::ob_tran, par::WGS84, par::sinu>("obt-au", amsterdam, utrecht, "+ellps=WGS84 +units=m +o_proj=sinu +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 5);
    test_forward<par::ob_tran, par::WGS84, par::sinu>("obt-au", amsterdam, utrecht, "+units=m +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 5);
    //test_forward<par::ob_tran_oblique, par::WGS84>("obto-ad", aspen, denver, "+ellps=WGS84 +units=m +o_proj=sinu +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 14);
    //test_forward<par::ob_tran_transverse, par::WGS84>("obtt-ad", aspen, denver, "+ellps=WGS84 +units=m +o_proj=sinu +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 6);
    //test_forward<par::ob_tran, par::WGS84, par::sinu>("obt-ad", aspen, denver, "+ellps=WGS84 +units=m +o_proj=sinu +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 14);
    test_forward<par::ob_tran, par::WGS84, par::sinu>("obt-ad", aspen, denver, "+units=m +o_lat_p=10 +o_lon_p=90 +o_lon_o=11.50", 14);

}

int test_main(int, char* [])
{
    test_all<double>();

    return 0;
}
