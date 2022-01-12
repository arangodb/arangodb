// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_equals_on_spheroid
#endif

#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include "test_equals.hpp"

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/core/cs.hpp>

namespace bgm = bg::model;

template <typename P1, typename P2 = P1>
struct test_point_point
{
    static inline void apply(std::string const& header)
    {
        std::string const str = header + "-";

        test_geometry<P1, P2>(str + "pp_01", "POINT(0 0)", "POINT(0 0)", true);
        test_geometry<P1, P2>(str + "pp_02", "POINT(0 0)", "POINT(10 0)", false);

        // points whose longitudes differ by 360 degrees
        test_geometry<P1, P2>(str + "pp_03", "POINT(0 0)", "POINT(360 0)", true);
        test_geometry<P1, P2>(str + "pp_04", "POINT(10 0)", "POINT(370 0)", true);
        test_geometry<P1, P2>(str + "pp_05", "POINT(10 0)", "POINT(-350 0)", true);
        test_geometry<P1, P2>(str + "pp_06", "POINT(180 10)", "POINT(-180 10)", true);
        test_geometry<P1, P2>(str + "pp_06a", "POINT(540 10)", "POINT(-540 10)", true);

#ifdef BOOST_GEOMETRY_NORMALIZE_LATITUDE
        test_geometry<P1, P2>(str + "pp_06b", "POINT(540 370)", "POINT(-540 -350)", true);
        test_geometry<P1, P2>(str + "pp_06c", "POINT(1260 370)", "POINT(-1260 -350)", true);
        test_geometry<P1, P2>(str + "pp_06d", "POINT(2340 370)", "POINT(-2340 -350)", true);
#endif

        test_geometry<P1, P2>(str + "pp_06e", "POINT(-180 10)", "POINT(-540 10)", true);
        test_geometry<P1, P2>(str + "pp_06f", "POINT(180 10)", "POINT(-540 10)", true);

        // north & south pole
        test_geometry<P1, P2>(str + "pp_07", "POINT(0 90)", "POINT(0 90)", true);

#ifdef BOOST_GEOMETRY_NORMALIZE_LATITUDE
        test_geometry<P1, P2>(str + "pp_07a", "POINT(0 450)", "POINT(10 -270)", true);
        test_geometry<P1, P2>(str + "pp_07b", "POINT(0 270)", "POINT(10 90)", false);
        test_geometry<P1, P2>(str + "pp_07c", "POINT(0 -450)", "POINT(10 90)", false);
#endif

        test_geometry<P1, P2>(str + "pp_08", "POINT(0 90)", "POINT(10 90)", true);
        test_geometry<P1, P2>(str + "pp_09", "POINT(0 90)", "POINT(0 -90)", false);
        test_geometry<P1, P2>(str + "pp_10", "POINT(0 -90)", "POINT(0 -90)", true);
        test_geometry<P1, P2>(str + "pp_11", "POINT(0 -90)", "POINT(10 -90)", true);
        test_geometry<P1, P2>(str + "pp_11a", "POINT(0 -90)", "POINT(10 90)", false);
        test_geometry<P1, P2>(str + "pp_12", "POINT(0 -90)", "POINT(0 -85)", false);
        test_geometry<P1, P2>(str + "pp_13", "POINT(0 90)", "POINT(0 85)", false);
        test_geometry<P1, P2>(str + "pp_14", "POINT(0 90)", "POINT(10 85)", false);

        // symmetric wrt prime meridian
        test_geometry<P1, P2>(str + "pp_15", "POINT(-10 45)", "POINT(10 45)", false);
        test_geometry<P1, P2>(str + "pp_16", "POINT(-170 45)", "POINT(170 45)", false);

        // other points
        test_geometry<P1, P2>(str + "pp_17", "POINT(-10 45)", "POINT(10 -45)", false);
        test_geometry<P1, P2>(str + "pp_18", "POINT(-10 -45)", "POINT(10 45)", false);
        test_geometry<P1, P2>(str + "pp_19", "POINT(10 -135)", "POINT(10 45)", false);

#ifdef BOOST_GEOMETRY_NORMALIZE_LATITUDE
        test_geometry<P1, P2>(str + "pp_20", "POINT(190 135)", "POINT(10 45)", true);
        test_geometry<P1, P2>(str + "pp_21", "POINT(190 150)", "POINT(10 30)", true);
        test_geometry<P1, P2>(str + "pp_21a", "POINT(-170 150)", "POINT(10 30)", true);
        test_geometry<P1, P2>(str + "pp_22", "POINT(190 -135)", "POINT(10 -45)", true);
        test_geometry<P1, P2>(str + "pp_23", "POINT(190 -150)", "POINT(10 -30)", true);
        test_geometry<P1, P2>(str + "pp_23a", "POINT(-170 -150)", "POINT(10 -30)", true);
#endif
    }
};


template <typename P1, typename P2 = P1>
struct test_point_point_with_height
{
    static inline void apply(std::string const& header)
    {
        std::string const str = header + "-";

        test_geometry<P1, P2>(str + "pp_01",
                              "POINT(0 0 10)",
                              "POINT(0 0 20)",
                              true);

        test_geometry<P1, P2>(str + "pp_02",
                              "POINT(0 0 10)",
                              "POINT(10 0 10)",
                              false);

        // points whose longitudes differ by 360 degrees
        test_geometry<P1, P2>(str + "pp_03",
                              "POINT(0 0 10)",
                              "POINT(360 0 10)",
                              true);

        // points whose longitudes differ by 360 degrees
        test_geometry<P1, P2>(str + "pp_04",
                              "POINT(10 0 10)",
                              "POINT(370 0 10)",
                              true);

        test_geometry<P1, P2>(str + "pp_05",
                              "POINT(10 0 10)",
                              "POINT(10 0 370)",
                              false);
    }
};


template <typename P>
void test_segment_segment(std::string const& header)
{
    typedef bgm::segment<P> seg;

    std::string const str = header + "-";

    test_geometry<seg, seg>(str + "ss_01",
                            "SEGMENT(10 0,180 0)",
                            "SEGMENT(10 0,-180 0)",
                            true);
    test_geometry<seg, seg>(str + "ss_02",
                            "SEGMENT(0 90,180 0)",
                            "SEGMENT(10 90,-180 0)",
                            true);
    test_geometry<seg, seg>(str + "ss_03",
                            "SEGMENT(0 90,0 -90)",
                            "SEGMENT(10 90,20 -90)",
                            true);
    test_geometry<seg, seg>(str + "ss_04",
                            "SEGMENT(10 80,10 -80)",
                            "SEGMENT(10 80,20 -80)",
                            false);
    test_geometry<seg, seg>(str + "ss_05",
                            "SEGMENT(170 10,-170 10)",
                            "SEGMENT(170 10,350 10)",
                            false);
}


BOOST_AUTO_TEST_CASE( equals_point_point_se )
{
    typedef bg::cs::spherical_equatorial<bg::degree> cs_type;

    test_point_point<bgm::point<int, 2, cs_type> >::apply("se");
    test_point_point<bgm::point<double, 2, cs_type> >::apply("se");
    test_point_point<bgm::point<long double, 2, cs_type> >::apply("se");

    // mixed point types
    test_point_point
        <
            bgm::point<double, 2, cs_type>, bgm::point<int, 2, cs_type>
        >::apply("se");

    test_point_point
        <
            bgm::point<double, 2, cs_type>, bgm::point<long double, 2, cs_type>
        >::apply("se");
}

BOOST_AUTO_TEST_CASE( equals_point_point_with_height_se )
{
    typedef bg::cs::spherical_equatorial<bg::degree> cs_type;

    test_point_point<bgm::point<int, 3, cs_type> >::apply("seh");
    test_point_point<bgm::point<double, 3, cs_type> >::apply("seh");
    test_point_point<bgm::point<long double, 3, cs_type> >::apply("seh");

    // mixed point types
    test_point_point
        <
            bgm::point<double, 3, cs_type>, bgm::point<int, 3, cs_type>
        >::apply("seh");

    test_point_point
        <
            bgm::point<double, 3, cs_type>, bgm::point<long double, 3, cs_type>
        >::apply("seh");
}

BOOST_AUTO_TEST_CASE( equals_point_point_geo )
{
    typedef bg::cs::geographic<bg::degree> cs_type;

    test_point_point<bgm::point<int, 2, cs_type> >::apply("geo");
    test_point_point<bgm::point<double, 2, cs_type> >::apply("geo");
    test_point_point<bgm::point<long double, 2, cs_type> >::apply("geo");

    // mixed point types
    test_point_point
        <
            bgm::point<double, 2, cs_type>, bgm::point<int, 2, cs_type>
        >::apply("se");

    test_point_point
        <
            bgm::point<double, 2, cs_type>, bgm::point<long double, 2, cs_type>
        >::apply("se");
}

BOOST_AUTO_TEST_CASE( equals_segment_segment_se )
{
    typedef bg::cs::spherical_equatorial<bg::degree> cs_type;

    test_segment_segment<bgm::point<int, 2, cs_type> >("se");
    test_segment_segment<bgm::point<double, 2, cs_type> >("se");
    test_segment_segment<bgm::point<long double, 2, cs_type> >("se");
}

BOOST_AUTO_TEST_CASE( equals_segment_segment_geo )
{
    typedef bg::cs::geographic<bg::degree> cs_type;

    test_segment_segment<bgm::point<int, 2, cs_type> >("geo");
    test_segment_segment<bgm::point<double, 2, cs_type> >("geo");
    test_segment_segment<bgm::point<long double, 2, cs_type> >("geo");
}

// This version uses collect_vectors (because its side
// strategy is spherical_side_formula) and fails
BOOST_AUTO_TEST_CASE( equals_ring_ring_se)
{
    using cs_type = bg::cs::spherical_equatorial<bg::degree> ;
    using ring_type = bgm::ring<bgm::point<double, 2, cs_type> >;

    test_geometry<ring_type, ring_type>("ring_simplex",
                                        "POLYGON((10 50,10 51,11 50,10 50))",
                                        "POLYGON((10 50,10 51,11 50,10 50))",
                                        true);
}

BOOST_AUTO_TEST_CASE( equals_ring_ring_geo )
{
    using cs_type = bg::cs::geographic<bg::degree> ;
    using ring_type = bgm::ring<bgm::point<double, 2, cs_type> >;

    test_geometry<ring_type, ring_type>("ring_simplex",
                                        "POLYGON((10 50,10 51,11 50,10 50))",
                                        "POLYGON((10 50,10 51,11 50,10 50))",
                                        true);

    test_geometry<ring_type, ring_type>("ring_simplex_false",
                                        "POLYGON((10 50,10 51,11 50,10 50))",
                                        "POLYGON((10 50,10 51.01,11 50,10 50))",
                                        false);
}
