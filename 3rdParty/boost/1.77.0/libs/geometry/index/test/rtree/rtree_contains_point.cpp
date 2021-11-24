// Boost.Geometry Index
// Unit Test

// Copyright (c) 2016 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <rtree/test_rtree.hpp>

#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/geometries.hpp>

template <typename Params>
void test_one()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> Pt;
    typedef bgi::rtree<Pt, Params> Rtree;
    Rtree rtree;

    rtree.insert(Pt(0, 0));
    rtree.insert(Pt(1, 1));
    rtree.insert(Pt(2, 2));
    rtree.insert(Pt(3, 3));
    rtree.insert(Pt(4, 4));
    rtree.insert(Pt(4, 3));
    rtree.insert(Pt(0, 3));

    for (typename Rtree::const_iterator it = rtree.begin() ; it != rtree.end() ; ++it)
    {
        std::vector<Pt> result;
        rtree.query(bgi::contains(*it), std::back_inserter(result));
        BOOST_CHECK(result.size() == 1);
    }
}

int test_main(int, char* [])
{
    test_one< bgi::linear<4> >();
    test_one< bgi::quadratic<4> >();
    test_one< bgi::rstar<4> >();

    return 0;
}
