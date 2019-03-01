// Boost.Geometry Index
// Unit Test

// Copyright (c) 2014 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <rtree/test_rtree.hpp>


template <typename Point, typename Params>
void test_rtree()
{
    bgi::rtree<Point, Params> rt;
    // coordinates aren't implicitly convertible to Point
    rt.insert(1.0);
    rt.remove(1.0);
}

int test_main(int, char* [])
{
    typedef bg::model::point<double, 1, bg::cs::cartesian> Pt;
    
    test_rtree<Pt, bgi::linear<16> >();
    test_rtree<Pt, bgi::quadratic<4> >();
    test_rtree<Pt, bgi::rstar<4> >();
    
    return 0;
}
