// Boost.Geometry

// Copyright (c) 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_CS_UNDEFINED_COMMON_HPP
#define BOOST_GEOMETRY_TEST_CS_UNDEFINED_COMMON_HPP

#include <geometry_test_common.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/io/wkt/read.hpp>

struct geom
{
    //typedef bg::model::point<double, 2, bg::cs::cartesian> point;
    typedef bg::model::point<double, 2, bg::cs::undefined> point;
    typedef bg::model::box<point> box;
    typedef bg::model::segment<point> segment;
    typedef bg::model::linestring<point> linestring;
    typedef bg::model::ring<point> ring;
    typedef bg::model::polygon<point> polygon;
    typedef bg::model::multi_linestring<linestring> multi_linestring;
    typedef bg::model::multi_polygon<polygon> multi_polygon;
    typedef bg::model::multi_point<point> multi_point;

    geom()
    {
        pt = point(0, 0);
        b = box(point(-1, -1), point(1, 1));
        s = segment(point(0, 0), point(1, 1));
        ls.push_back(point(0, 0));
        ls.push_back(point(1, 1));
        ls.push_back(point(1.1, 1.1));
        r.push_back(point(0, 0));
        r.push_back(point(0, 1));
        r.push_back(point(1, 1));
        r.push_back(point(1, 0));
        r.push_back(point(0, 0));
        po.outer() = r;
        po.inners().push_back(ring());
        po.inners().back().push_back(point(0, 0));
        po.inners().back().push_back(point(0.2, 0.1));
        po.inners().back().push_back(point(0.1, 0.2));
        po.inners().back().push_back(point(0, 0));
        mls.push_back(ls);
        mpo.push_back(po);
        mpt.push_back(pt);
    }

    point pt;
    box b;
    segment s;
    linestring ls;
    ring r;
    polygon po;
    multi_linestring mls;
    multi_polygon mpo;
    multi_point mpt;
};

#endif // BOOST_GEOMETRY_TEST_CS_UNDEFINED_COMMON_HPP
