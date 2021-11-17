// Boost.Geometry
// Unit Test

// Copyright (c) 2017 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include "test_formula.hpp"
#include "vertex_longitude_cases.hpp"

#include <boost/geometry/formulas/vertex_latitude.hpp>
#include <boost/geometry/formulas/vertex_longitude.hpp>
#include <boost/geometry/formulas/vincenty_inverse.hpp>
#include <boost/geometry/formulas/thomas_inverse.hpp>
#include <boost/geometry/formulas/andoyer_inverse.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/util/math.hpp>

#define BOOST_GEOMETRY_TEST_DEBUG

namespace bg = boost::geometry;

template<typename CT>
CT test_vrt_lon_sph(CT lon1r,
                    CT lat1r,
                    CT lon2r,
                    CT lat2r)
{
    CT a1 = bg::formula::spherical_azimuth(lon1r, lat1r, lon2r, lat2r);

    typedef bg::model::point<CT, 2,
            bg::cs::spherical_equatorial<bg::radian> > point;

    bg::model::segment<point> segment(point(lon1r, lat1r),
                                      point(lon2r, lat2r));
    bg::model::box<point> box;
    bg::envelope(segment, box);

    CT vertex_lat;
    CT lat_sum = lat1r + lat2r;
    if (lat_sum > CT(0))
    {
        vertex_lat = bg::get_as_radian<bg::max_corner, 1>(box);
    } else {
        vertex_lat = bg::get_as_radian<bg::min_corner, 1>(box);
    }

    bg::strategy::azimuth::spherical<> azimuth;

    return bg::formula::vertex_longitude
            <CT, bg::spherical_equatorial_tag>::
            apply(lon1r, lat1r,
                  lon2r, lat2r,
                  vertex_lat,
                  a1,
                  azimuth);
}

template
<
        template <typename, bool, bool, bool, bool, bool> class FormulaPolicy,
        typename CT
        >
CT test_vrt_lon_geo(CT lon1r,
                    CT lat1r,
                    CT lon2r,
                    CT lat2r)
{
    // WGS84
    bg::srs::spheroid<CT> spheroid(6378137.0, 6356752.3142451793);

    typedef FormulaPolicy<CT, false, true, false, false, false> formula;
    CT a1 = formula::apply(lon1r, lat1r, lon2r, lat2r, spheroid).azimuth;

    typedef bg::model::point<CT, 2, bg::cs::geographic<bg::radian> > geo_point;

    bg::model::segment<geo_point> segment(geo_point(lon1r, lat1r),
                                          geo_point(lon2r, lat2r));
    bg::model::box<geo_point> box;
    bg::envelope(segment, box);

    CT vertex_lat;
    CT lat_sum = lat1r + lat2r;
    if (lat_sum > CT(0))
    {
        vertex_lat = bg::get_as_radian<bg::max_corner, 1>(box);
    } else {
        vertex_lat = bg::get_as_radian<bg::min_corner, 1>(box);
    }

    bg::strategy::azimuth::geographic<> azimuth_geographic;

    return bg::formula::vertex_longitude
            <CT, bg::geographic_tag>::apply(lon1r, lat1r,
                                            lon2r, lat2r,
                                            vertex_lat,
                                            a1,
                                            azimuth_geographic);

}

void test_all(expected_results const& results)
{
    double const d2r = bg::math::d2r<double>();

    double lon1r = results.p1.lon * d2r;
    double lat1r = results.p1.lat * d2r;
    double lon2r = results.p2.lon * d2r;
    double lat2r = results.p2.lat * d2r;

    if(lon1r > lon2r)
    {
        std::swap(lon1r, lon2r);
        std::swap(lat1r, lat2r);
    }

    double res_an = test_vrt_lon_geo<bg::formula::andoyer_inverse>
            (lon1r, lat1r, lon2r, lat2r);
    double res_th = test_vrt_lon_geo<bg::formula::thomas_inverse>
            (lon1r, lat1r, lon2r, lat2r);
    double res_vi = test_vrt_lon_geo<bg::formula::vincenty_inverse>
            (lon1r, lat1r, lon2r, lat2r);
    double res_sh = test_vrt_lon_sph(lon1r, lat1r, lon2r, lat2r);

    bg::math::normalize_longitude<bg::radian, double>(res_an);
    bg::math::normalize_longitude<bg::radian, double>(res_th);
    bg::math::normalize_longitude<bg::radian, double>(res_vi);
    bg::math::normalize_longitude<bg::radian, double>(res_sh);

    check_one(res_an, results.andoyer * d2r, res_vi, 0.001);
    check_one(res_th, results.thomas * d2r, res_vi, 0.01);//in some tests thomas gives low accuracy
    check_one(res_vi, results.vincenty * d2r, res_vi, 0.0000001);
    check_one(res_sh, results.spherical * d2r, res_vi, 1);
}

int test_main(int, char*[])
{

    for (size_t i = 0; i < expected_size; ++i)
    {
        test_all(expected[i]);
    }

    return 0;
}

