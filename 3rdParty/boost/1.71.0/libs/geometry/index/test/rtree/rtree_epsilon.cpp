// Boost.Geometry Index
// Unit Test

// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Enable enlargement of Values' bounds by epsilon in the rtree
// for Points and Segments
#define BOOST_GEOMETRY_INDEX_EXPERIMENTAL_ENLARGE_BY_EPSILON

#include <vector>

#include <rtree/test_rtree.hpp>

#include <boost/geometry/geometries/register/point.hpp>
#include <boost/geometry/geometries/polygon.hpp>

template <typename Params>
void test_rtree(unsigned vcount)
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;

    std::vector<point_t> values;

    double eps = std::numeric_limits<double>::epsilon();
    values.push_back(point_t(eps/2, eps/2));

    for ( unsigned i = 1 ; i < vcount ; ++i )
    {
        values.push_back(point_t(i, i));
    }

    point_t qpt(0, 0);

    BOOST_CHECK(bg::intersects(qpt, values[0]));

    {
        bgi::rtree<point_t, Params> rt(values);

        std::vector<point_t> result;
        rt.query(bgi::intersects(qpt), std::back_inserter(result));
        BOOST_CHECK(result.size() == 1);

        rt.remove(values.begin() + vcount/2, values.end());

        result.clear();
        rt.query(bgi::intersects(qpt), std::back_inserter(result));
        BOOST_CHECK(result.size() == 1);
    }
    
    {
        bgi::rtree<point_t, Params> rt;
        rt.insert(values);
        
        std::vector<point_t> result;
        rt.query(bgi::intersects(qpt), std::back_inserter(result));
        BOOST_CHECK(result.size() == 1);

        rt.remove(values.begin() + vcount/2, values.end());

        result.clear();
        rt.query(bgi::intersects(qpt), std::back_inserter(result));
        BOOST_CHECK(result.size() == 1);
    }
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
