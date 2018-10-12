// Boost.Geometry
// Unit Test

// Copyright (c) 2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html


#include <geometry_test_common.hpp>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>


int test_main(int, char* [])
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point;
    typedef bg::model::box<point> box;
    typedef bg::model::linestring<point> linestring;
    typedef bg::model::multi_linestring<linestring> mlinestring;
    typedef bg::model::polygon<point> polygon;    
    typedef bg::model::multi_polygon<polygon> mpolygon;

    point p;
    linestring ls;
    mlinestring mls;
    polygon po;
    mpolygon mpo;

    bg::read_wkt("POINT(0 0)", p);
    bg::read_wkt("LINESTRING(0 0,7 7,7 9)", ls);
    bg::read_wkt("MULTILINESTRING((0 0,7 7,7 9),(7 9, 9 9))", mls);
    bg::read_wkt("POLYGON((0 0,0 5,5 5,5 0,0 0),(1 1,4 1,4 4,1 4,1 1))", po);
    bg::read_wkt("MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(1 1,4 1,4 4,1 4,1 1)),((2 2,2 3,3 3,3 2,2 2)))", mpo);

    BOOST_CHECK_CLOSE(bg::perimeter(po), 32.0, 0.0001);
    BOOST_CHECK_CLOSE(bg::area(mpo), 17.0, 0.0001);
    BOOST_CHECK_CLOSE(bg::length(mls), 13.899494936611665, 0.0001);

    BOOST_CHECK(bg::covered_by(p, po));
    BOOST_CHECK(!bg::crosses(ls, mls));
    BOOST_CHECK(!bg::equals(ls, mls));
    BOOST_CHECK(bg::intersects(ls, po));
    BOOST_CHECK(bg::relate(p, ls, bg::de9im::mask("F0F******")));
    BOOST_CHECK(bg::relation(mls, mpo).str() == "101F00212");
    BOOST_CHECK(bg::within(po, mpo));
    BOOST_CHECK(!bg::touches(mls, po));
    
    mpolygon res;
    bg::intersection(po, mpo, res);
    BOOST_CHECK_CLOSE(bg::area(res), 16.0, 0.0001);
    bg::clear(res);
    bg::union_(po, mpo, res);
    BOOST_CHECK_CLOSE(bg::area(res), 17.0, 0.0001);
    bg::clear(res);
    bg::difference(mpo, po, res);
    BOOST_CHECK_CLOSE(bg::area(res), 1.0, 0.0001);
    bg::clear(res);
    bg::sym_difference(mpo, po, res);
    BOOST_CHECK_CLOSE(bg::area(res), 1.0, 0.0001);

    BOOST_CHECK(bg::is_simple(ls));
    BOOST_CHECK(bg::is_valid(mpo));

    point c;
    bg::centroid(mpo, c);
    BOOST_CHECK_CLOSE(bg::distance(p, c), 3.5355339059327378, 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(mls, mpo), 0.0, 0.0001);
    BOOST_CHECK_CLOSE(bg::distance(po, mpo), 0.0, 0.0001);

    box b;
    bg::envelope(mls, b);
    BOOST_CHECK_CLOSE(bg::area(b), 81.0, 0.0001);

    polygon h;
    bg::convex_hull(mls, h);
    BOOST_CHECK_CLOSE(bg::area(h), 9.0, 0.0001);

    return 0;
}

