// Boost.Geometry
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[multi_polygon
//` Declaration and use of the Boost.Geometry model::multi_polygon, modelling the MultiPolygon Concept

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

namespace bg = boost::geometry;

int main()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
    typedef bg::model::polygon<point_t> polygon_t; /*< Default parameters, clockwise, closed polygon. >*/
    typedef bg::model::multi_polygon<polygon_t> mpolygon_t; /*< Clockwise, closed multi_polygon. >*/

    mpolygon_t mpoly1; /*< Default-construct a multi_polygon. >*/

#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) \
 && !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

    mpolygon_t mpoly2{{{{0.0, 0.0}, {0.0, 5.0}, {5.0, 5.0}, {5.0, 0.0}, {0.0, 0.0}},
                       {{1.0, 1.0}, {4.0, 1.0}, {4.0, 4.0}, {1.0, 4.0}, {1.0, 1.0}}},
                      {{{5.0, 5.0}, {5.0, 6.0}, {6.0, 6.0}, {6.0, 5.0}, {5.0, 5.0}}}}; /*< Construct a multi_polygon containing two polygons, using C++11 unified initialization syntax. >*/

#endif

    mpoly1.resize(2); /*< Resize a multi_polygon, store two polygons. >*/

    bg::append(mpoly1[0].outer(), point_t(0.0, 0.0)); /*< Append point to the exterior ring of the first polygon. >*/
    bg::append(mpoly1[0].outer(), point_t(0.0, 5.0));
    bg::append(mpoly1[0].outer(), point_t(5.0, 5.0));
    bg::append(mpoly1[0].outer(), point_t(5.0, 0.0));
    bg::append(mpoly1[0].outer(), point_t(0.0, 0.0));

    mpoly1[0].inners().resize(1); /*< Resize a container of interior rings of the first polygon. >*/
    bg::append(mpoly1[0].inners()[0], point_t(1.0, 1.0)); /*< Append point to the interior ring of the first polygon. >*/
    bg::append(mpoly1[0].inners()[0], point_t(4.0, 1.0));
    bg::append(mpoly1[0].inners()[0], point_t(4.0, 4.0));
    bg::append(mpoly1[0].inners()[0], point_t(1.0, 4.0));
    bg::append(mpoly1[0].inners()[0], point_t(1.0, 1.0));

    bg::append(mpoly1[1].outer(), point_t(5.0, 5.0)); /*< Append point to the exterior ring of the second polygon. >*/
    bg::append(mpoly1[1].outer(), point_t(5.0, 6.0));
    bg::append(mpoly1[1].outer(), point_t(6.0, 6.0));
    bg::append(mpoly1[1].outer(), point_t(6.0, 5.0));
    bg::append(mpoly1[1].outer(), point_t(5.0, 5.0));

    double a = bg::area(mpoly1);

    std::cout << a << std::endl;

    return 0;
}

//]


//[multi_polygon_output
/*`
Output:
[pre
17
]
*/
//]
