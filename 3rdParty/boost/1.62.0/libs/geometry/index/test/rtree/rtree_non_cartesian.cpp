// Boost.Geometry Index
// Unit Test

// Copyright (c) 2016 Adam Wulkiewicz, Lodz, Poland.

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

template <typename Point>
void test_cs()
{
    test_value<Point>();
    test_value<bg::model::box<Point> >();
}

int test_main(int, char* [])
{
    //test_cs<bg::model::point<double, 2, bg::cs::cartesian> >();
    test_cs<bg::model::point<double, 2, bg::cs::spherical_equatorial<bg::degree> > >();
    test_cs<bg::model::point<double, 2, bg::cs::geographic<bg::degree> > >();

    return 0;
}
