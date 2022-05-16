// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2020 Baidyanath Kundu, Haldia, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[from_wkt
//` Shows the usage of from_wkt

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>

int main()
{
    using point_type = boost::geometry::model::d2::point_xy<double>;
    using box_type = boost::geometry::model::box<point_type>;
    using segment_type = boost::geometry::model::segment<point_type>;
    using polygon_type = boost::geometry::model::polygon<point_type>;
    using linestring_type = boost::geometry::model::linestring<point_type>;

    auto const a = boost::geometry::from_wkt<point_type>("POINT(1 2)");
    auto const d = boost::geometry::from_wkt<box_type>("BOX(0 0,3 3)");
    auto const e = boost::geometry::from_wkt<segment_type>("SEGMENT(1 0,3 4)");
    auto const c = boost::geometry::from_wkt<polygon_type>("POLYGON((0 0,0 7,4 2,2 0,0 0))");
    auto const b = boost::geometry::from_wkt<linestring_type>("LINESTRING(0 0,2 2,3 1)");

    return 0;
}

//]
