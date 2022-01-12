// Boost.Geometry
// QuickBook Example

// Copyright (c) 2011-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2015 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[multi_linestring
//` Declaration and use of the Boost.Geometry model::multi_linestring, modelling the MultiLinestring Concept

#include <iostream>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>

namespace bg = boost::geometry;

int main()
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
    typedef bg::model::linestring<point_t> linestring_t;
    typedef bg::model::multi_linestring<linestring_t> mlinestring_t;

    mlinestring_t mls1; /*< Default-construct a multi_linestring. >*/

#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) \
 && !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

    mlinestring_t mls2{{{0.0, 0.0}, {0.0, 1.0}, {2.0, 1.0}},
                       {{1.0, 0.0}, {2.0, 0.0}}}; /*< Construct a multi_linestring containing two linestrings, using C++11 unified initialization syntax. >*/

#endif

    mls1.resize(2); /*< Resize a multi_linestring, store two linestrings. >*/

    bg::append(mls1[0], point_t(0.0, 0.0)); /*< Append point to the first linestring. >*/
    bg::append(mls1[0], point_t(0.0, 1.0));
    bg::append(mls1[0], point_t(2.0, 1.0));

    bg::append(mls1[1], point_t(1.0, 0.0)); /*< Append point to the second linestring. >*/
    bg::append(mls1[1], point_t(2.0, 0.0));

    double l = bg::length(mls1);

    std::cout << l << std::endl;

    return 0;
}

//]


//[multi_linestring_output
/*`
Output:
[pre
4
]
*/
//]
