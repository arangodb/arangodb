// Boost.Geometry

// Copyright (c) 2016-2020 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_within.hpp"


#include <boost/geometry/geometries/geometries.hpp>


template <typename Point>
void test_point_box_by_side()
{
    // Test spherical boxes
    // See also http://www.gcmap.com/mapui?P=1E45N-19E45N-19E55N-1E55N-1E45N,10E55.1N,10E45.1N
    typedef bg::model::box<Point> box_t;
    std::conditional_t
        <
            std::is_same<typename bg::cs_tag<Point>::type, bg::geographic_tag>::value,
            bg::strategy::within::geographic_point_box_by_side<>,
            bg::strategy::within::spherical_point_box_by_side<>
        > by_side;
    box_t box;
    bg::read_wkt("POLYGON((1 45,19 55))", box);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 55.1), box, by_side), true);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 55.2), box, by_side), true);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 55.3), box, by_side), true);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 55.4), box, by_side), false);

    BOOST_CHECK_EQUAL(bg::within(Point(10, 45.1), box, by_side), false);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 45.2), box, by_side), false);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 45.3), box, by_side), false);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 45.4), box, by_side), true);

    bg::strategy::within::spherical_point_box s;
    BOOST_CHECK_EQUAL(bg::within(Point(10, 44.9), box, s), false);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 45.0), box, s), false);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 45.1), box, s), true);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 49.9), box, s), true);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 55.0), box, s), false);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 55.1), box, s), false);    

    // By default Box is not a polygon in spherical CS, edges are defined by small circles
    BOOST_CHECK_EQUAL(bg::within(Point(10, 45.1), box), true);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 54.9), box), true);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 55), box), false);
    BOOST_CHECK_EQUAL(bg::within(Point(10, 45), box), false);

    // Crossing the dateline (Near Tuvalu)
    // http://www.gcmap.com/mapui?P=178E10S-178W10S-178W6S-178E6S-178E10S,180W5.999S,180E9.999S
    // http://en.wikipedia.org/wiki/Tuvalu

    box_t tuvalu(Point(178, -10), Point(-178, -6));
    BOOST_CHECK_EQUAL(bg::within(Point(180, -8), tuvalu, by_side), true);
    BOOST_CHECK_EQUAL(bg::within(Point(-180, -8), tuvalu, by_side), true);
    BOOST_CHECK_EQUAL(bg::within(Point(180, -5.999), tuvalu, by_side), false);
    BOOST_CHECK_EQUAL(bg::within(Point(180, -10.001), tuvalu, by_side), true);

    // The above definition of a Box is not valid
    // min should be lesser than max
    // By default Box is not a polygon in spherical CS, edges are defined by small circles
    box_t tuvalu2(Point(178, -10), Point(182, -6));
    BOOST_CHECK_EQUAL(bg::within(Point(180, -8), tuvalu2), true);
    BOOST_CHECK_EQUAL(bg::within(Point(-180, -8), tuvalu2), true);
    BOOST_CHECK_EQUAL(bg::within(Point(180, -6.001), tuvalu2), true);
    BOOST_CHECK_EQUAL(bg::within(Point(180, -5.999), tuvalu2), false);
    BOOST_CHECK_EQUAL(bg::within(Point(180, -9.999), tuvalu2), true);
    BOOST_CHECK_EQUAL(bg::within(Point(180, -10.001), tuvalu2), false);
}


template <typename P>
void test_point_box()
{
    typedef bg::model::box<P> box_t;

    test_geometry<P, box_t>("POINT(0 0)",    "BOX(0 0, 1 1)", false);
    test_geometry<P, box_t>("POINT(1 1)",    "BOX(0 0, 2 2)", true);

    test_geometry<P, box_t>("POINT(180 1)",  "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(-180 1)", "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(180 1)",  "BOX(170 0, 180 2)", false);
    test_geometry<P, box_t>("POINT(-180 1)", "BOX(170 0, 180 2)", false);
    test_geometry<P, box_t>("POINT(179 1)",  "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(-179 1)", "BOX(170 0, 190 2)", true);
    test_geometry<P, box_t>("POINT(179 1)",  "BOX(170 0, 180 2)", true);
    test_geometry<P, box_t>("POINT(-179 1)", "BOX(170 0, 180 2)", false);
    test_geometry<P, box_t>("POINT(169 1)",  "BOX(170 0, 180 2)", false);

    test_point_box_by_side<P>();
}

template <typename P>
void test_box_box()
{
    typedef bg::model::box<P> box_t;

    test_geometry<box_t, box_t>("BOX(0 0, 1 1)", "BOX(0 0, 1 1)", true);

    test_geometry<box_t, box_t>("BOX(-170 0,-160 1)", "BOX(-180 0, 180 1)", true);
    test_geometry<box_t, box_t>("BOX(-170 0,-160 1)", "BOX(170 0, 200 1)",  true);
    test_geometry<box_t, box_t>("BOX(-170 0,-150 1)", "BOX(170 0, 200 1)",  false);
    test_geometry<box_t, box_t>("BOX(0 0,1 1)",       "BOX(170 0, 370 1)",  true);
    test_geometry<box_t, box_t>("BOX(0 0,10 1)",      "BOX(170 0, 370 1)",  true);
    test_geometry<box_t, box_t>("BOX(-180 0,10 1)",   "BOX(170 0, 370 1)",  true);
    test_geometry<box_t, box_t>("BOX(-180 0,20 1)",   "BOX(170 0, 370 1)",  false);
    test_geometry<box_t, box_t>("BOX(10 0,20 1)",     "BOX(170 0, 370 1)",  false);
    test_geometry<box_t, box_t>("BOX(160 0,180 1)",   "BOX(170 0, 370 1)",  false);

    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(180 0, 190 1)",  true); // invalid?
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(180 0, 191 1)",  true); // invalid?
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(179 0, 190 1)",  true);
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(181 0, 190 1)",  false); // invalid?
    test_geometry<box_t, box_t>("BOX(-180 0,-170 1)", "BOX(180 0, 189 1)",  false); // invalid?
}


template <typename P>
void test_cs()
{
    test_point_box<P>();
    test_box_box<P>();
}


int test_main( int , char* [] )
{
    test_cs<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_cs<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

    return 0;
}
