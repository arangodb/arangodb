// Boost.Geometry
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[linestring
//` Declaration and use of the Boost.Geometry model::linestring, modelling the Linestring Concept

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

namespace bg = boost::geometry;

int main()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
    typedef bg::model::linestring<point_t> linestring_t;

    linestring_t ls1; /*< Default-construct a linestring. >*/

#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) \
 && !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

    linestring_t ls2{{0.0, 0.0}, {1.0, 0.0}, {1.0, 2.0}}; /*< Construct a linestring containing three points, using C++11 unified initialization syntax. >*/

#endif

    bg::append(ls1, point_t(0.0, 0.0)); /*< Append point. >*/
    bg::append(ls1, point_t(1.0, 0.0));
    bg::append(ls1, point_t(1.0, 2.0));

    double l = bg::length(ls1);

    std::cout << l << std::endl;

    return 0;
}

//]


//[linestring_output
/*`
Output:
[pre
3
]
*/
//]
