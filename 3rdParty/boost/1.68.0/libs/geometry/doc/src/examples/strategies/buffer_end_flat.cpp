// Boost.Geometry (aka GGL, Generic Geometry Library)
// QuickBook Example

// Copyright (c) 2014 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[buffer_end_flat
//` Shows how the end_flat strategy can be used as a EndStrategy to create flat ends

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/geometries.hpp>
/*<-*/ #include "../examples_utils/create_svg_buffer.hpp" /*->*/

int main()
{
    typedef boost::geometry::model::d2::point_xy<double> point;
    typedef boost::geometry::model::linestring<point> linestring;
    typedef boost::geometry::model::polygon<point> polygon;

    // Declare the flat-end strategy
    boost::geometry::strategy::buffer::end_flat end_strategy;

    // Declare other strategies
    boost::geometry::strategy::buffer::distance_symmetric<double> distance_strategy(1.0);
    boost::geometry::strategy::buffer::side_straight side_strategy;
    boost::geometry::strategy::buffer::join_round join_strategy;
    boost::geometry::strategy::buffer::point_circle point_strategy;

    // Declare/fill a multi linestring
    boost::geometry::model::multi_linestring<linestring> ml;
    boost::geometry::read_wkt("MULTILINESTRING((3 5,5 10,7 5),(7 7,11 10,15 7,19 10))", ml);

    // Create the buffered geometry with flat ends
    boost::geometry::model::multi_polygon<polygon> result;
    boost::geometry::buffer(ml, result,
                distance_strategy, side_strategy,
                join_strategy, end_strategy, point_strategy);
    /*<-*/ create_svg_buffer("buffer_end_flat.svg", ml, result); /*->*/

    return 0;
}

//]

