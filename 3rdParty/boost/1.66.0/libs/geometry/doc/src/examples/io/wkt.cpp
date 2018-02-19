// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2014 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[wkt
//` Shows the usage of wkt

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

int main()
{
    namespace geom = boost::geometry;
    typedef geom::model::d2::point_xy<double> point_type;

    point_type point = geom::make<point_type>(3, 6);
    geom::model::polygon<point_type> polygon;
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(0, 0));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(0, 4));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(4, 4));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(4, 0));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(0, 0));

    std::cout << boost::geometry::wkt(point) << std::endl;
    std::cout << boost::geometry::wkt(polygon) << std::endl;

    return 0;
}

//]


//[wkt_output
/*`
Output:
[pre
POINT(3 6)
POLYGON((0 0,0 4,4 4,4 0,0 0))
]


*/
//]
