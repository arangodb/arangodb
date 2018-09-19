// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2017.
// Modifications copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#if defined(_MSC_VER)
#pragma warning( disable : 4305 ) // truncation double -> float
#endif // defined(_MSC_VER)


#define BOOST_GEOMETRY_SRS_ENABLE_STATIC_PROJECTION_HYBRID_INTERFACE

#include <geometry_test_common.hpp>

#include <boost/geometry/srs/projection.hpp>

#include <boost/geometry/algorithms/transform.hpp>
#include <boost/geometry/core/coordinate_type.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <test_common/test_point.hpp>


namespace srs = bg::srs;
namespace par = bg::srs::par4;

template <typename Prj, typename Model, typename P1, typename P2>
void test_one(double lon, double lat,
              typename bg::coordinate_type<P2>::type x,
              typename bg::coordinate_type<P2>::type y,
              std::string const& parameters)
{
    // hybrid interface disabled by default
    // static_proj4 default ctor, dynamic parameters passed
    srs::projection<srs::static_proj4<par::proj<Prj>, par::ellps<Model> > >
        prj = srs::proj4(parameters);

    P1 ll;
    bg::set<0>(ll, lon);
    bg::set<1>(ll, lat);

    P2 xy;
    prj.forward(ll, xy);

    BOOST_CHECK_CLOSE(bg::get<0>(xy), x, 0.001);
    BOOST_CHECK_CLOSE(bg::get<1>(xy), y, 0.001);
}

template <typename P>
void test_all()
{
    typedef typename bg::coordinate_type<P>::type coord_type;
    typedef bg::model::point<coord_type, 2, bg::cs::geographic<bg::degree> > point_type;

    // aea
    test_one<par::aea, par::WGS84, point_type, P>
        (4.897000, 52.371000, 334609.583974, 5218502.503686,
         "+proj=aea +ellps=WGS84 +units=m +lat_1=55 +lat_2=65");
}

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(bg::cs::cartesian)

int test_main(int, char* [])
{
    //test_all<int[2]>();
    test_all<float[2]>();
    test_all<double[2]>();

    // 2D -> 3D
    //test_all<test::test_point>();
    
    //test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();
    test_all<bg::model::d2::point_xy<long double> >();

    return 0;
}
