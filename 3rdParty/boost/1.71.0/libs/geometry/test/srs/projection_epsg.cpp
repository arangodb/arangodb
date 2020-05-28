// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2017, 2018.
// Modifications copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER)
#pragma warning( disable : 4305 ) // truncation double -> float
#endif // defined(_MSC_VER)


#include <boost/core/ignore_unused.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/srs/epsg.hpp>
#include <boost/geometry/srs/projection.hpp>

#include <boost/geometry/core/coordinate_type.hpp>

#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <test_common/test_point.hpp>

namespace srs = bg::srs;

template <int E, typename P1, typename P2>
void test_one(double lon, double lat,
              typename bg::coordinate_type<P2>::type x,
              typename bg::coordinate_type<P2>::type y)
{
    srs::projection<srs::static_epsg<E> > prj;
    
    P1 ll;
    bg::set<0>(ll, lon);
    bg::set<1>(ll, lat);

    P2 xy;
    bg::set<0>(xy, 0.0);
    bg::set<1>(xy, 0.0);
    prj.forward(ll, xy);

    BOOST_CHECK_CLOSE(bg::get<0>(xy), x, 0.001);
    BOOST_CHECK_CLOSE(bg::get<1>(xy), y, 0.001);
}

template <typename D, typename P>
void test_deg_rad(double factor)
{
    typedef typename bg::coordinate_type<P>::type coord_type;
    typedef bg::model::point<coord_type, 2, bg::cs::geographic<D> > point_type;

    // sterea
    test_one<28992, point_type, P>(4.897000 * factor, 52.371000 * factor, 121590.388077, 487013.903377);
    // utm
    test_one<29118, point_type, P>(4.897000 * factor, 52.371000 * factor, 4938115.7568751378, 9139797.6057944782);
}

template <typename P>
void test_all()
{
    test_deg_rad<bg::degree, P>(1.0);
    test_deg_rad<bg::radian, P>(bg::math::d2r<double>());
}

int test_main(int, char* [])
{
    // Commented out most the types because otherwise it cannot be linked
    //test_all<int[2]>();
    //test_all<float[2]>();
    //test_all<double[2]>();
    //test_all<test::test_point>();
    //test_all<bg::model::d2::point_xy<int> >();
    ////test_all<bg::model::d2::point_xy<float> >();
    ////test_all<bg::model::d2::point_xy<long double> >();

    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
