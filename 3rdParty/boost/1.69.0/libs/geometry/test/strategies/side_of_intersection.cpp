// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2011-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/strategies/cartesian/side_of_intersection.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/segment.hpp>


namespace bg = boost::geometry;

int test_main(int, char* [])
{
    typedef bg::model::d2::point_xy<boost::long_long_type> point;
    typedef bg::model::segment<point> segment;

    typedef bg::strategy::side::side_of_intersection side;

    point no_fb(-99, -99);

    segment a(point(20, 10), point(10, 20));

    segment b1(point(11, 16), point(20, 14));  // IP with a: (14.857, 15.143)
    segment b2(point(10, 16), point(20, 14));  // IP with a: (15, 15)

    segment c1(point(15, 16), point(13, 8));
    segment c2(point(15, 16), point(14, 8));
    segment c3(point(15, 16), point(15, 8));


    BOOST_CHECK_EQUAL( 1, side::apply(a, b1, c1, no_fb));
    BOOST_CHECK_EQUAL(-1, side::apply(a, b1, c2, no_fb));
    BOOST_CHECK_EQUAL(-1, side::apply(a, b1, c3, no_fb));

    BOOST_CHECK_EQUAL( 1, side::apply(a, b2, c1, no_fb));
    BOOST_CHECK_EQUAL( 1, side::apply(a, b2, c2, no_fb));
    BOOST_CHECK_EQUAL( 0, side::apply(a, b2, c3, no_fb));

    // Collinear cases
    // 1: segments intersecting are collinear (with a fallback point):
    {
        point fb(5, 5);
        segment col1(point(0, 5), point(5, 5));
        segment col2(point(5, 5), point(10, 5)); // One IP with col1 at (5,5)
        segment col3(point(5, 0), point(5, 5));
        BOOST_CHECK_EQUAL( 0, side::apply(col1, col2, col3, fb));
    }
    // 2: segment of side calculation collinear with one of the segments
    {
        point fb(5, 5);
        segment col1(point(0, 5), point(10, 5));
        segment col2(point(5, 5), point(5, 12)); // IP with col1 at (5,5)
        segment col3(point(5, 1), point(5, 5)); // collinear with col2
        BOOST_CHECK_EQUAL( 0, side::apply(col1, col2, col3, fb));
    }
    {
        point fb(5, 5);
        segment col1(point(10, 5), point(0, 5));
        segment col2(point(5, 5), point(5, 12)); // IP with col1 at (5,5)
        segment col3(point(5, 1), point(5, 5)); // collinear with col2
        BOOST_CHECK_EQUAL( 0, side::apply(col1, col2, col3, fb));
    }
    {
        point fb(5, 5);
        segment col1(point(0, 5), point(10, 5));
        segment col2(point(5, 12), point(5, 5)); // IP with col1 at (5,5)
        segment col3(point(5, 1), point(5, 5)); // collinear with col2
        BOOST_CHECK_EQUAL( 0, side::apply(col1, col2, col3, fb));
    }

    {
        point fb(517248, -517236);
        segment col1(point(-172408, -517236), point(862076, -517236));
        segment col2(point(517248, -862064), point(517248, -172408));
        segment col3(point(517248, -172408), point(517248, -517236));
        BOOST_CHECK_EQUAL( 0, side::apply(col1, col2, col3, fb));
    }
    {
        point fb(-221647, -830336);
        segment col1(point(-153817, -837972), point(-222457, -830244));
        segment col2(point(-221139, -833615), point(-290654, -384388));
        segment col3(point(-255266, -814663), point(-264389, -811197));
        BOOST_CHECK_EQUAL(1, side::apply(col1, col2, col3, fb)); // Left of segment...
    }


    {
        point fb(27671131, 30809240);
        segment col1(point(27671116, 30809247), point(27675474, 30807351));
        segment col2(point(27666779, 30811130), point(27671139, 30809237));
        segment col3(point(27671122, 30809244), point(27675480, 30807348));
        BOOST_CHECK_EQUAL(1, side::apply(col1, col2, col3, fb)); // Left of segment...
    }

    // TODO: we might add a check calculating the IP, determining the side
    // with the normal side strategy, and verify the results are equal

    return 0;
}

