// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
//
// This file was modified by Oracle on 2015, 2016.
// Modifications copyright (c) 2015-2016, Oracle and/or its affiliates.
//
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <algorithms/area/test_area.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>




template <typename CT>
void test_all()
{
    typedef typename bg::model::d2::point_xy<CT> pt_crt;
    typedef typename bg::model::point<CT, 2, bg::cs::spherical_equatorial<bg::degree> > pt_sph;
    typedef typename bg::model::point<CT, 2, bg::cs::geographic<bg::degree> > pt_geo;

    typedef bg::model::multi_polygon<bg::model::polygon<pt_crt> > mp_crt;
    typedef bg::model::multi_polygon<bg::model::polygon<pt_sph> > mp_sph;
    typedef bg::model::multi_polygon<bg::model::polygon<pt_geo> > mp_geo;

    // mean Earth's radius^2
    double r2 = bg::math::sqr(bg::get_radius<0>(bg::srs::sphere<double>()));

    std::string poly = "MULTIPOLYGON(((0 0,0 7,4 2,2 0,0 0)))";
    test_geometry<mp_crt>(poly, 16.0);
    test_geometry<mp_sph>(poly, 197897454752.69489 / r2);
    test_geometry<mp_geo>(poly, 197018888665.8331);
}

int test_main( int , char* [] )
{
    test_all<double>();

#ifdef HAVE_TTMATH
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
