// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2014 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[read_wkt
//` Shows the usage of read_wkt

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point_type;

    point_type a;
    boost::geometry::model::linestring<point_type> b;
    boost::geometry::model::polygon<point_type> c;
    boost::geometry::model::box<point_type> d;
    boost::geometry::model::segment<point_type> e;

    boost::geometry::read_wkt("POINT(1 2)", a);
    boost::geometry::read_wkt("LINESTRING(0 0,2 2,3 1)", b);
    boost::geometry::read_wkt("POLYGON((0 0,0 7,4 2,2 0,0 0))", c);
    boost::geometry::read_wkt("BOX(0 0,3 3)", d);
    boost::geometry::read_wkt("SEGMENT(1 0,3 4)", e);

    return 0;
}

//]

