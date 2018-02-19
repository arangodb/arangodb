// Boost.Geometry Index
// Unit Test

// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <rtree/test_rtree.hpp>

#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>

template <typename Params>
void test_rtree(unsigned vcount)
{
    typedef bg::model::point<int, 1, bg::cs::cartesian> point_t;

    bgi::rtree<point_t, Params> rt;

    BOOST_CHECK(rt.remove(point_t(0)) == 0);

    for ( unsigned i = 0 ; i < vcount ; ++i )
    {
        rt.insert(point_t(static_cast<int>(i)));
    }

    BOOST_CHECK(rt.size() == vcount);
    BOOST_CHECK(rt.count(point_t(vcount / 2)) == 1);

    for ( unsigned i = 0 ; i < vcount + 3 ; ++i )
    {
        rt.remove(point_t((i + 3) % vcount));
    }

    BOOST_CHECK(rt.size() == 0);
    BOOST_CHECK(rt.count(point_t(vcount / 2)) == 0);

    for ( unsigned i = 0 ; i < vcount ; ++i )
    {
        rt.insert(point_t((i + 5) % vcount));
    }

    BOOST_CHECK(rt.size() == vcount);
    BOOST_CHECK(rt.count(point_t(vcount / 2)) == 1);

    for ( unsigned i = 0 ; i < vcount + 3 ; ++i )
    {
        rt.remove(point_t((i + 7) % vcount));
    }

    BOOST_CHECK(rt.size() == 0);
    BOOST_CHECK(rt.count(point_t(vcount / 2)) == 0);
}

template <int Max, int Min>
void test_rtree_all()
{
    int pow = Max;
    for (int l = 0 ; l < 3 ; ++l)
    {
        pow *= Max;
        int vcount = (pow * 8) / 10;

        //std::cout << Max << " " << Min << " " << vcount << std::endl;

        test_rtree< bgi::linear<Max, Min> >(vcount);
        test_rtree< bgi::quadratic<Max, Min> >(vcount);
        test_rtree< bgi::rstar<Max, Min> >(vcount);
    }
}

int test_main(int, char* [])
{
    test_rtree_all<2, 1>();
    test_rtree_all<4, 1>();
    test_rtree_all<4, 2>();
    test_rtree_all<5, 3>();

    return 0;
}
