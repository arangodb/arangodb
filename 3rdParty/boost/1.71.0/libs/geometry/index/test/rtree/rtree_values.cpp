// Boost.Geometry Index
// Unit Test

// Copyright (c) 2014-2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <rtree/test_rtree.hpp>

#include <boost/core/addressof.hpp>

#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>

struct point
{
    point(double xx = 0, double yy = 0) : x(xx), y(yy) {}
    double x, y;
};

BOOST_GEOMETRY_REGISTER_POINT_2D(point, double, bg::cs::cartesian, x, y)

template <typename Rtree, typename Convertible>
void check_convertible_to_value(Rtree const& rt, Convertible const& conv)
{
    static const bool
        is_conv_to_indexable
            = boost::is_convertible<Convertible, typename Rtree::indexable_type>::value;
    static const bool
        is_conv_to_value
            = boost::is_convertible<Convertible, typename Rtree::value_type>::value;
    static const bool
        is_same_as_indexable
            = boost::is_same<Convertible, typename Rtree::indexable_type>::value;
    static const bool
        is_same_as_value
            = boost::is_same<Convertible, typename Rtree::value_type>::value;

    BOOST_CHECK_EQUAL(is_same_as_indexable, false);
    BOOST_CHECK_EQUAL(is_same_as_value, false);
    BOOST_CHECK_EQUAL(is_conv_to_indexable, false);
    BOOST_CHECK_EQUAL(is_conv_to_value, true);

    typename Rtree::value_type const& val = conv;
    BOOST_CHECK(rt.value_eq()(val, conv));
}

template <typename Box, typename Params>
void test_pair()
{
    typedef std::pair<Box, std::size_t> Value;

    typename boost::remove_const<Box>::type box;
    bg::assign_zero(box);

    Value val(box, 0);

    // sanity check
    std::vector<Value> vec;
    vec.push_back(val);
    vec.push_back(std::make_pair(box, 0));
    vec.push_back(std::make_pair(box, (unsigned short)0));

    bgi::rtree<Value, Params> rt;
    rt.insert(val);
    rt.insert(std::make_pair(box, 0));
    rt.insert(std::make_pair(box, (unsigned short)0));
    BOOST_CHECK_EQUAL(rt.size(), 3u);

    check_convertible_to_value(rt, std::make_pair(box, 0));
    check_convertible_to_value(rt, std::make_pair(box, (unsigned short)0));
    BOOST_CHECK(bg::covered_by(rt.indexable_get()(std::make_pair(box, 0)), rt.bounds()));
    BOOST_CHECK(bg::covered_by(rt.indexable_get()(std::make_pair(box, (unsigned short)0)), rt.bounds()));

    BOOST_CHECK_EQUAL(rt.count(val), 3u);
    BOOST_CHECK_EQUAL(rt.count(std::make_pair(box, 0)), 3u);
    BOOST_CHECK_EQUAL(rt.count(std::make_pair(box, (unsigned short)0)), 3u);
    BOOST_CHECK_EQUAL(rt.count(box), 3u);

    BOOST_CHECK_EQUAL(rt.remove(val), 1u);
    BOOST_CHECK_EQUAL(rt.remove(std::make_pair(box, 0)), 1u);
    BOOST_CHECK_EQUAL(rt.remove(std::make_pair(box, (unsigned short)0)), 1u);
    BOOST_CHECK_EQUAL(rt.size(), 0u);
}

template <typename Box, typename Params>
void test_pair_geom_ptr()
{
    typedef typename bg::point_type<Box>::type point_t;
    typedef bg::model::polygon<point_t> polygon_t;

    typedef std::pair<Box, polygon_t*> Value;

    typename boost::remove_const<Box>::type box;
    bg::assign_zero(box);

    polygon_t poly;

    Value val(box, boost::addressof(poly));

    bgi::rtree<Value, Params> rt;
    rt.insert(val);
    rt.insert(std::make_pair(box, boost::addressof(poly)));

    BOOST_CHECK_EQUAL(rt.size(), 2u);

    BOOST_CHECK_EQUAL(rt.remove(val), 1u);
    BOOST_CHECK_EQUAL(rt.remove(std::make_pair(box, boost::addressof(poly))), 1u);

    BOOST_CHECK_EQUAL(rt.size(), 0u);
}

template <typename Params>
void test_point()
{
    bgi::rtree<point, Params> rt;
    
    rt.insert(0.0);
    BOOST_CHECK_EQUAL(rt.size(), 1u);
    BOOST_CHECK_EQUAL(rt.remove(0.0), 1u);
}

int test_main(int, char* [])
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> Pt;
    typedef bg::model::box<Pt> Box;

    test_pair< Box, bgi::linear<16> >();
    test_pair< Box, bgi::quadratic<4> >();
    test_pair< Box, bgi::rstar<4> >();
    //test_rtree< Box const, bgi::linear<16> >();
    //test_rtree< Box const, bgi::quadratic<4> >();
    //test_rtree< Box const, bgi::rstar<4> >();

    test_pair_geom_ptr< Box, bgi::linear<16> >();
    test_pair_geom_ptr< Box, bgi::quadratic<4> >();
    test_pair_geom_ptr< Box, bgi::rstar<4> >();

    test_point< bgi::linear<16> >();
    test_point< bgi::quadratic<4> >();
    test_point< bgi::rstar<4> >();

    return 0;
}
