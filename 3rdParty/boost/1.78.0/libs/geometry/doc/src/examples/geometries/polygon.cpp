// Boost.Geometry
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[polygon
//` Declaration and use of the Boost.Geometry model::polygon, modelling the Polygon Concept

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

namespace bg = boost::geometry;

int main()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
    typedef bg::model::polygon<point_t> polygon_t; /*< Default parameters, clockwise, closed polygon. >*/

    polygon_t poly1; /*< Default-construct a polygon. >*/

#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) \
 && !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

    polygon_t polygon2{{{0.0, 0.0}, {0.0, 5.0}, {5.0, 5.0}, {5.0, 0.0}, {0.0, 0.0}},
                       {{1.0, 1.0}, {4.0, 1.0}, {4.0, 4.0}, {1.0, 4.0}, {1.0, 1.0}}}; /*< Construct a polygon containing an exterior and interior ring, using C++11 unified initialization syntax. >*/

#endif

    bg::append(poly1.outer(), point_t(0.0, 0.0)); /*< Append point to the exterior ring. >*/
    bg::append(poly1.outer(), point_t(0.0, 5.0));
    bg::append(poly1.outer(), point_t(5.0, 5.0));
    bg::append(poly1.outer(), point_t(5.0, 0.0));
    bg::append(poly1.outer(), point_t(0.0, 0.0));

    poly1.inners().resize(1); /*< Resize a container of interior rings. >*/
    bg::append(poly1.inners()[0], point_t(1.0, 1.0)); /*< Append point to the interior ring. >*/
    bg::append(poly1.inners()[0], point_t(4.0, 1.0));
    bg::append(poly1.inners()[0], point_t(4.0, 4.0));
    bg::append(poly1.inners()[0], point_t(1.0, 4.0));
    bg::append(poly1.inners()[0], point_t(1.0, 1.0));

    double a = bg::area(poly1);

    std::cout << a << std::endl;

    return 0;
}

//]


//[polygon_output
/*`
Output:
[pre
16
]
*/
//]
