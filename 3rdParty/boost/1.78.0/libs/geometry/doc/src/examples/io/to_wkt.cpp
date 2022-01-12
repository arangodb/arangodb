// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2020 Baidyanath Kundu, Haldia, India.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[to_wkt
//` Shows the usage of to_wkt

#include <iostream>

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

int main()
{
    namespace geom = boost::geometry;
    typedef geom::model::d2::point_xy<double> point_type;

    point_type point = geom::make<point_type>(3, 2);
    geom::model::polygon<point_type> polygon;
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(0, 0));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(0, 4));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(4, 4));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(4, 0));
    geom::append(geom::exterior_ring(polygon), geom::make<point_type>(0, 0));

    std::cout << boost::geometry::to_wkt(point) << std::endl;
    std::cout << boost::geometry::to_wkt(polygon) << std::endl;

    point_type point_frac = geom::make<point_type>(3.141592654, 27.18281828);
    geom::model::polygon<point_type> polygon_frac;
    geom::append(geom::exterior_ring(polygon_frac), geom::make<point_type>(0.00000, 0.00000));
    geom::append(geom::exterior_ring(polygon_frac), geom::make<point_type>(0.00000, 4.00001));
    geom::append(geom::exterior_ring(polygon_frac), geom::make<point_type>(4.00001, 4.00001));
    geom::append(geom::exterior_ring(polygon_frac), geom::make<point_type>(4.00001, 0.00000));
    geom::append(geom::exterior_ring(polygon_frac), geom::make<point_type>(0.00000, 0.00000));

    std::cout << boost::geometry::to_wkt(point_frac, 3) << std::endl;
    std::cout << boost::geometry::to_wkt(polygon_frac, 3) << std::endl;

    return 0;
}

//]


//[to_wkt_output
/*`
Output:
[pre
POINT(3 2)
POLYGON((0 0,0 4,4 4,4 0,0 0))
POINT(3.14 27.2)
POLYGON((0 0,0 4,4 4,4 0,0 0))
]


*/
//]
