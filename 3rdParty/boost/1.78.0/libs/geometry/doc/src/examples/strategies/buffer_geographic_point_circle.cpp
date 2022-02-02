// Boost.Geometry
// QuickBook Example

// Copyright (c) 2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[buffer_geographic_point_circle
//` Shows how the point_circle strategy, for the Earth, can be used as a PointStrategy to create circular buffers around points

#include <boost/geometry.hpp>
#include <iostream>

int main()
{
    namespace bg = boost::geometry;

    typedef bg::model::point<double, 2, bg::cs::geographic<bg::degree> > point;

    // Declare the geographic_point_circle strategy (with 36 points)
    // Default template arguments (taking Andoyer strategy)
    bg::strategy::buffer::geographic_point_circle<> point_strategy(36);

    // Declare the distance strategy (one kilometer, around the point, on Earth)
    bg::strategy::buffer::distance_symmetric<double> distance_strategy(1000.0);

    // Declare other necessary strategies, unused for point
    bg::strategy::buffer::join_round join_strategy;
    bg::strategy::buffer::end_round end_strategy;
    bg::strategy::buffer::side_straight side_strategy;

    // Declare/fill a point on Earth, near Amsterdam
    point p;
    bg::read_wkt("POINT(4.9 52.1)", p);

    // Create the buffer of a point on the Earth
    bg::model::multi_polygon<bg::model::polygon<point> > result;
    bg::buffer(p, result,
                distance_strategy, side_strategy,
                join_strategy, end_strategy, point_strategy);

    std::cout << "Area: " << bg::area(result) / (1000 * 1000) << " square kilometer" << std::endl;

    return 0;
}

//]

//[buffer_geographic_point_circle_output
/*`
Output:
[pre
Area: 3.12542 square kilometer
]
*/
//]
