// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// Copyright (c) 2014-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_is_valid_geo
#endif

#include <limits>
#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include "test_is_valid.hpp"

#include <boost/geometry/core/coordinate_type.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/reverse.hpp>


BOOST_AUTO_TEST_CASE( test_is_valid_geo_polygon )
{
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > pt;
    typedef bg::model::polygon<pt, false> G;

    typedef validity_tester_geo_areal<false> tester;
    typedef test_valid<tester, G> test;

    test::apply("p01", "POLYGON((-1  -1, 1  -1, 1  1, -1  1, -1  -1),(-0.5 -0.5, -0.5 0.5, 0.0 0.0, -0.5 -0.5),(0.0 0.0, 0.5 0.5, 0.5 -0.5, 0.0 0.0))", true);
}

template <typename Poly, typename Spheroid>
void test_valid_s(std::string const& wkt,
                  Spheroid const& sph,
                  bool expected_result)
{
    bg::strategy::intersection::geographic_segments<> is(sph);
    bg::strategy::area::geographic<> as(sph);

    Poly p;
    bg::read_wkt(wkt, p);
    bg::correct(p, as);

    BOOST_CHECK(bg::is_valid(p, is) == expected_result);
}

BOOST_AUTO_TEST_CASE( test_is_valid_epsg4053_polygon )
{
    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > pt;
    typedef bg::model::polygon<pt, false> po;

    bg::srs::spheroid<double> epsg4053(6371228, 6371228);
    bg::srs::spheroid<double> wgs84;

    // NOTE: the orientation is different in these CSes,
    // in one of them the polygon is corrected before passing it into is_valid()

    test_valid_s<po>("POLYGON((-148 -68,178 -74,76 0,-148 -68))", wgs84, true);
    test_valid_s<po>("POLYGON((-148 -68,178 -74,76 0,-148 -68))", epsg4053, true);

    test_valid_s<po>("POLYGON((-152 -54,-56 43,142 -52,-152 -54))", wgs84, true);
    test_valid_s<po>("POLYGON((-152 -54,-56 43,142 -52,-152 -54))", epsg4053, true);

    return;
}