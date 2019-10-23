// Boost.Geometry
// Unit Test

// Copyright (c) 2014-2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <geometry_test_common.hpp>

#include <boost/assign.hpp>
#include <boost/range.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/geometries/register/point.hpp>

#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/algorithms/num_geometries.hpp>

typedef std::pair<float, float> pt_pair_t;
BOOST_GEOMETRY_REGISTER_POINT_2D(pt_pair_t, float, bg::cs::cartesian, first, second)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)

template <typename P>
void test_default()
{
    typedef bg::model::multi_point<P> mpt;
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    // multi_point()
    mpt mptd;
    BOOST_CHECK(bg::num_points(mptd) == 0);
    // linestring()
    ls lsd;
    BOOST_CHECK(bg::num_points(lsd) == 0);
    // multi_linestring()
    mls mlsd;
    BOOST_CHECK(bg::num_points(mlsd) == 0);
    // ring()
    ring rd;
    BOOST_CHECK(bg::num_points(rd) == 0);
    // polygon()
    poly pd;
    BOOST_CHECK(bg::num_points(pd) == 0);
    // multi_polygon()
    mpoly mpd;
    BOOST_CHECK(bg::num_points(mpd) == 0);
}

template <typename P>
void test_boost_assign_2d()
{
    typedef bg::model::multi_point<P> mpt;
    typedef bg::model::linestring<P> ls;
    typedef bg::model::ring<P> ring;

    // using Boost.Assign
    mpt mpt2 = boost::assign::list_of(P(0, 0))(P(1, 0));
    BOOST_CHECK(bg::num_points(mpt2) == 2);
    mpt2 = boost::assign::list_of(P(0, 0))(P(1, 0));
    BOOST_CHECK(bg::num_points(mpt2) == 2);

    // using Boost.Assign
    ls ls2 = boost::assign::list_of(P(0, 0))(P(1, 0))(P(1, 1));
    BOOST_CHECK(bg::num_points(ls2) == 3);
    ls2 = boost::assign::list_of(P(0, 0))(P(1, 0))(P(1, 1));
    BOOST_CHECK(bg::num_points(ls2) == 3);

    // using Boost.Assign
    ring r2 = boost::assign::list_of(P(0, 0))(P(0, 1))(P(1, 1))(P(1, 0))(P(0, 0));
    BOOST_CHECK(bg::num_points(r2) == 5);
    r2 = boost::assign::list_of(P(0, 0))(P(0, 1))(P(1, 1))(P(1, 0))(P(0, 0));
    BOOST_CHECK(bg::num_points(r2) == 5);
}

void test_boost_assign_pair_2d()
{
    typedef std::pair<float, float> pt;

    test_boost_assign_2d<pt>();

    typedef bg::model::multi_point<pt> mpt;

    // using Boost.Assign
    mpt mpt2 = boost::assign::pair_list_of(0, 0)(1, 0);
    BOOST_CHECK(bg::num_points(mpt2) == 2);
    mpt2 = boost::assign::pair_list_of(0, 0)(1, 0);
    BOOST_CHECK(bg::num_points(mpt2) == 2);
}

void test_boost_assign_tuple_2d()
{
    typedef boost::tuple<float, float> pt;

    test_boost_assign_2d<pt>();

    typedef bg::model::multi_point<pt> mpt;

    // using Boost.Assign
    mpt mpt2 = boost::assign::tuple_list_of(0, 0)(1, 0);
    BOOST_CHECK(bg::num_points(mpt2) == 2);
    mpt2 = boost::assign::tuple_list_of(0, 0)(1, 0);
    BOOST_CHECK(bg::num_points(mpt2) == 2);
}

template <typename P>
void test_initializer_list_2d()
{
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST) && !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX)

    typedef bg::model::multi_point<P> mpt;
    typedef bg::model::linestring<P> ls;
    typedef bg::model::multi_linestring<ls> mls;
    typedef bg::model::ring<P> ring;
    typedef bg::model::polygon<P> poly;
    typedef bg::model::multi_polygon<poly> mpoly;

    // multi_point(initializer_list<Point>)
    mpt mpt1 = {{0, 0}, {1, 0}, {2, 0}};
    BOOST_CHECK(bg::num_geometries(mpt1) == 3);
    BOOST_CHECK(bg::num_points(mpt1) == 3);
    // multi_point::operator=(initializer_list<Point>)
    mpt1 = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};
    BOOST_CHECK(bg::num_points(mpt1) == 4);

    // linestring(initializer_list<Point>)
    ls ls1 = {{0, 0}, {1, 0}, {2, 0}};
    BOOST_CHECK(bg::num_geometries(ls1) == 1);
    BOOST_CHECK(bg::num_points(ls1) == 3);
    // linestring::operator=(initializer_list<Point>)
    ls1 = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};
    BOOST_CHECK(bg::num_points(ls1) == 4);

    // multi_linestring(initializer_list<Linestring>)
    mls mls1 = {{{0, 0}, {1, 0}, {2, 0}}, {{3, 0}, {4, 0}}};
    BOOST_CHECK(bg::num_geometries(mls1) == 2);
    BOOST_CHECK(bg::num_points(mls1) == 5);
    // multi_linestring::operator=(initializer_list<Linestring>)
    mls1 = {{{0, 0}, {1, 0}, {2, 0}}, {{3, 0}, {4, 0}, {5, 0}}};
    BOOST_CHECK(bg::num_points(mls1) == 6);

    // ring(initializer_list<Point>)
    ring r1 = {{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}};
    BOOST_CHECK(bg::num_geometries(r1) == 1);
    BOOST_CHECK(bg::num_points(r1) == 5);
    // ring::operator=(initializer_list<Point>)
    r1 = {{0, 0}, {0, 1}, {1, 2}, {2, 1}, {1, 0}, {0, 0}};
    BOOST_CHECK(bg::num_points(r1) == 6);

    // polygon(initializer_list<ring_type>)
    poly p1 = {{{0, 0}, {0, 9}, {9, 9}, {9, 0}, {0, 0}}, {{1, 1}, {2, 1}, {2, 2}, {1, 2}, {1, 1}}};
    BOOST_CHECK(bg::num_geometries(p1) == 1);
    BOOST_CHECK(bg::num_points(p1) == 10);
    BOOST_CHECK(boost::size(bg::interior_rings(p1)) == 1);
    // polygon::operator=(initializer_list<ring_type>)
    p1 = {{{0, 0}, {0, 8}, {8, 9}, {9, 8}, {8, 0}, {0, 0}}, {{1, 1}, {2, 1}, {2, 2}, {1, 2}, {1, 1}}};
    BOOST_CHECK(bg::num_points(p1) == 11);
    BOOST_CHECK(boost::size(bg::interior_rings(p1)) == 1);
    p1 = {{{0, 0}, {0, 9}, {9, 9}, {9, 0}, {0, 0}}};
    BOOST_CHECK(bg::num_points(p1) == 5);
    BOOST_CHECK(boost::size(bg::interior_rings(p1)) == 0);
    // polygon(initializer_list<ring_type>)
    poly p2 = {{{0, 0}, {0, 9}, {9, 9}, {9, 0}, {0, 0}}};
    BOOST_CHECK(bg::num_geometries(p2) == 1);
    BOOST_CHECK(bg::num_points(p2) == 5);
    BOOST_CHECK(boost::size(bg::interior_rings(p1)) == 0);
    // polygon::operator=(initializer_list<ring_type>)
    p2 = {{{0, 0}, {0, 8}, {8, 9}, {9, 8}, {8, 0}, {0, 0}}};
    BOOST_CHECK(bg::num_points(p2) == 6);

    // multi_polygon(initializer_list<Polygon>)
    mpoly mp1 = {{{{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}}}, {{{2, 2}, {2, 3}, {3, 3}, {3, 2}, {2, 2}}}};
    BOOST_CHECK(bg::num_geometries(mp1) == 2);
    BOOST_CHECK(bg::num_points(mp1) == 10);
    // multi_polygon::operator=(initializer_list<Polygon>)
    mp1 = {{{{0, 0}, {0, 1}, {1, 2}, {2, 1}, {1, 0}, {0, 0}}}, {{{2, 2}, {2, 3}, {3, 3}, {3, 2}, {2, 2}}}};
    BOOST_CHECK(bg::num_points(mp1) == 11);

#endif
}

template <typename P>
void test_all_2d()
{
    test_default<P>();
    test_boost_assign_2d<P>();
    test_initializer_list_2d<P>();
}

template <typename T>
struct test_point
{
    test_point(T = T(), T = T()) {}
};

template <typename T>
struct test_range
{
    test_range() {}
    template <typename It>
    test_range(It, It) {}
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
    test_range(std::initializer_list<T>) {}
    //test_range & operator=(std::initializer_list<T>) { return *this; }
#endif
};

void test_sanity_check()
{
    typedef test_point<float> P;
    typedef test_range<P> R;
    typedef std::vector<P> V;

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST) && !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX)
    {
        R r = {{1, 1},{2, 2},{3, 3}};
        r = {{1, 1},{2, 2},{3, 3}};

        V v = {{1, 1},{2, 2},{3, 3}};
        v = {{1, 1},{2, 2},{3, 3}};
    }
#endif
    {
        R r = boost::assign::list_of(P(1, 1))(P(2, 2))(P(3, 3));
        r = boost::assign::list_of(P(1, 1))(P(2, 2))(P(3, 3));

        V v = boost::assign::list_of(P(1, 1))(P(2, 2))(P(3, 3));
        //v = boost::assign::list_of(P(1, 1))(P(2, 2))(P(3, 3));
        v.empty();
    }
}

int test_main(int, char* [])
{
    test_all_2d< bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all_2d< bg::model::d2::point_xy<float> >();

    test_boost_assign_pair_2d();
    test_boost_assign_tuple_2d();

    test_sanity_check();

    return 0;
}

