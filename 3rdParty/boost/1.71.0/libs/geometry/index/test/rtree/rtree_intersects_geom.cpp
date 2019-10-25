// Boost.Geometry Index
// Unit Test

// Copyright (c) 2016 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <rtree/test_rtree.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

template <typename Value, typename Point, typename Params>
void test_all()
{
    typedef bg::model::box<Point> Box;
    typedef bg::model::segment<Point> Seg;
    typedef bg::model::ring<Point> Ring;
    typedef bg::model::polygon<Point> Poly;
    typedef bg::model::multi_polygon<Poly> MPoly;
    typedef bg::model::linestring<Point> Ls;
    typedef bg::model::multi_linestring<Ls> MLs;
    typedef bg::model::multi_point<Point> MPt;

    bgi::rtree<Value, Params> rt;
    std::vector<Value> found;
    
    rt.query(bgi::intersects(Point()), back_inserter(found));
    rt.query(bgi::intersects(Seg()), back_inserter(found));
    rt.query(bgi::intersects(Box()), back_inserter(found));
    rt.query(bgi::intersects(Ring()), back_inserter(found));
    rt.query(bgi::intersects(Poly()), back_inserter(found));
    rt.query(bgi::intersects(MPoly()), back_inserter(found));
    rt.query(bgi::intersects(Ls()), back_inserter(found));
    rt.query(bgi::intersects(MLs()), back_inserter(found));
    rt.query(bgi::intersects(MPt()), back_inserter(found));
}

int test_main(int, char* [])
{
    typedef bg::model::d2::point_xy<double> Pt;
    typedef bg::model::box<Pt> Box;

    test_all< Pt, Pt, bgi::linear<16> >();
    test_all< Pt, Pt, bgi::quadratic<4> >();
    test_all< Pt, Pt, bgi::rstar<4> >();

    test_all< Box, Pt, bgi::linear<16> >();
    test_all< Box, Pt, bgi::quadratic<4> >();
    test_all< Box, Pt, bgi::rstar<4> >();
    
    return 0;
}
