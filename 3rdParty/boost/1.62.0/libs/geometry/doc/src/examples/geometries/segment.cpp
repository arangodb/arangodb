// Boost.Geometry
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[segment
//` Declaration and use of the Boost.Geometry model::segment, modelling the Segment Concept

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

namespace bg = boost::geometry;

int main()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
    typedef bg::model::segment<point_t> segment_t;

    segment_t seg1; /*< Default-construct a segment. >*/
    segment_t seg2(point_t(0.0, 0.0), point_t(5.0, 5.0)); /*< Construct, assigning the first and the second point. >*/

#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX

    segment_t seg3{{0.0, 0.0}, {5.0, 5.0}}; /*< Construct, using C++11 unified initialization syntax. >*/

#endif

    bg::set<0, 0>(seg1, 1.0); /*< Set a coordinate. >*/
    bg::set<0, 1>(seg1, 2.0);
    bg::set<1, 0>(seg1, 3.0);
    bg::set<1, 1>(seg1, 4.0);

    double x0 = bg::get<0, 0>(seg1); /*< Get a coordinate. >*/
    double y0 = bg::get<0, 1>(seg1);
    double x1 = bg::get<1, 0>(seg1);
    double y1 = bg::get<1, 1>(seg1);

    std::cout << x0 << ", " << y0 << ", " << x1 << ", " << y1 << std::endl;

    return 0;
}

//]


//[segment_output
/*`
Output:
[pre
1, 2, 3, 4
]
*/
//]
