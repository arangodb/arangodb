// Boost.Geometry Index
// Unit Test

// Copyright (c) 2016 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <rtree/test_rtree.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/io/wkt/read.hpp>

template <typename Point>
inline void fill(Point & pt, double x, double y)
{
    bg::set<0>(pt, x);
    bg::set<1>(pt, y);
}

template <typename Point>
inline void fill(bg::model::box<Point> & box, double x, double y)
{
    bg::set<0, 0>(box, x);
    bg::set<0, 1>(box, y);
    bg::set<1, 0>(box, x + 20);
    bg::set<1, 1>(box, y + 20);
}

template <typename Rtree>
void test_rtree()
{
    typedef typename Rtree::value_type value_t;

    Rtree rtree;
    value_t v;

    // This is not fully valid because both point's longitude and box's min
    // longitude should be in range [-180, 180]. So if this stopped passing
    // in the future it wouldn't be that bad. Still it works as it is now.

    size_t n = 0;
    for (double x = -170; x < 400; x += 30, ++n)
    {
        //double lon = x <= 180 ? x : x - 360;
        double lon = x;
        double lat = x <= 180 ? 0 : 30;

        fill(v, lon, lat);
        rtree.insert(v);
        BOOST_CHECK_EQUAL(rtree.size(), n + 1);
        size_t vcount = 1; // x < 180 ? 1 : 2;
        BOOST_CHECK_EQUAL(rtree.count(v), vcount);
        std::vector<value_t> res;
        rtree.query(bgi::intersects(v), std::back_inserter(res));
        BOOST_CHECK_EQUAL(res.size(), vcount);
    }

    for (double x = -170; x < 400; x += 30, --n)
    {
        //double lon = x <= 180 ? x : x - 360;
        double lon = x;
        double lat = x <= 180 ? 0 : 30;

        fill(v, lon, lat);
        rtree.remove(v);
        BOOST_CHECK_EQUAL(rtree.size(), n - 1);
        size_t vcount = 0; // x < 180 ? 1 : 0;
        BOOST_CHECK_EQUAL(rtree.count(v), vcount);
        std::vector<value_t> res;
        rtree.query(bgi::intersects(v), std::back_inserter(res));
        BOOST_CHECK_EQUAL(res.size(), vcount);
    }
}

template <typename Value>
void test_value()
{
    test_rtree<bgi::rtree<Value, bgi::linear<4> > >();
    test_rtree<bgi::rtree<Value, bgi::quadratic<4> > >();
    test_rtree<bgi::rtree<Value, bgi::rstar<4> > >();
}

template <typename Rtree>
void test_ticket_12413()
{
    typedef typename Rtree::value_type pair_t;
    typedef typename pair_t::first_type point_t;

    Rtree rtree;
    rtree.insert(std::make_pair(point_t(-1.558444, 52.38664), 792));
    rtree.insert(std::make_pair(point_t(-1.558444, 52.38664), 793));
    rtree.insert(std::make_pair(point_t(-2.088824, 51.907406), 800));
    rtree.insert(std::make_pair(point_t(-1.576363, 53.784089), 799));
    rtree.insert(std::make_pair(point_t(-77.038816, 38.897282), 801));
    rtree.insert(std::make_pair(point_t(-1.558444, 52.38664), 794));
    rtree.insert(std::make_pair(point_t(-0.141588, 51.501009), 797));
    rtree.insert(std::make_pair(point_t(-118.410468, 34.103003), 798));
    rtree.insert(std::make_pair(point_t(-0.127592, 51.503407), 796));

    size_t num_removed = rtree.remove(std::make_pair(point_t(-0.127592, 51.503407), 796));

    BOOST_CHECK_EQUAL(num_removed, 1u);
}

template <typename Point>
void test_cs()
{
    test_value<Point>();
    test_value<bg::model::box<Point> >();

    {
        typedef std::pair<Point, unsigned> value_t;
        test_ticket_12413<bgi::rtree<value_t, bgi::linear<4> > >();
        test_ticket_12413<bgi::rtree<value_t, bgi::quadratic<4> > >();
        test_ticket_12413<bgi::rtree<value_t, bgi::rstar<4> > >();
    }
}

int test_main(int, char* [])
{
    //test_cs<bg::model::point<double, 2, bg::cs::cartesian> >();
    test_cs<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_cs<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

    return 0;
}
