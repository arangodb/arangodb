// Boost.Geometry
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[multi_point
//` Declaration and use of the Boost.Geometry model::multi_point, modelling the MultiPoint Concept

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

namespace bg = boost::geometry;

int main()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
    typedef bg::model::multi_point<point_t> mpoint_t;

    mpoint_t mpt1; /*< Default-construct a multi_point. >*/

#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) \
 && !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

    mpoint_t mpt2{{{0.0, 0.0}, {1.0, 1.0}, {2.0, 2.0}}}; /*< Construct a multi_point containing three points, using C++11 unified initialization syntax. >*/

#endif

    bg::append(mpt1, point_t(0.0, 0.0)); /*< Append point to the multi_point. >*/
    bg::append(mpt1, point_t(1.0, 1.0));
    bg::append(mpt1, point_t(2.0, 2.0));

    std::size_t count = bg::num_points(mpt1);

    std::cout << count << std::endl;

    return 0;
}

//]


//[multi_point_output
/*`
Output:
[pre
3
]
*/
//]
