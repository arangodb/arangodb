// Boost.Geometry
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[box
//` Declaration and use of the Boost.Geometry model::box, modelling the Box Concept

#include <iostream>
#include <boost/geometry.hpp>

namespace bg = boost::geometry;

int main()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
    typedef bg::model::box<point_t> box_t;

    box_t box1; /*< Default-construct a box. >*/
    box_t box2(point_t(0.0, 0.0), point_t(5.0, 5.0)); /*< Construct, assigning min and max corner point. >*/

#ifndef BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX

    box_t box3{{0.0, 0.0}, {5.0, 5.0}}; /*< Construct, using C++11 unified initialization syntax. >*/

#endif

    bg::set<bg::min_corner, 0>(box1, 1.0); /*< Set a coordinate, generic. >*/
    bg::set<bg::min_corner, 1>(box1, 2.0);
    box1.max_corner().set<0>(3.0); /*< Set a coordinate, class-specific ([*Note]: prefer `bg::set()`). >*/
    box1.max_corner().set<1>(4.0);

    double min_x = bg::get<bg::min_corner, 0>(box1); /*< Get a coordinate, generic. >*/
    double min_y = bg::get<bg::min_corner, 1>(box1);
    double max_x = box1.max_corner().get<0>(); /*< Get a coordinate, class-specific ([*Note]: prefer `bg::get()`). >*/
    double max_y = box1.max_corner().get<1>();

    std::cout << min_x << ", " << min_y << ", " << max_x << ", " << max_y << std::endl;

    return 0;
}

//]


//[box_output
/*`
Output:
[pre
1, 2, 3, 4
]
*/
//]
