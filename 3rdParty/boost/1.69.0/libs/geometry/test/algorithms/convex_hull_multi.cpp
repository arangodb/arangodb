// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014, 2015.
// Modifications copyright (c) 2014-2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <iterator>
#include <string>

#include <algorithms/test_convex_hull.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/point_type.hpp>

#include <boost/geometry/views/detail/range_type.hpp>

#include <boost/geometry/algorithms/detail/for_each_range.hpp>

#include <boost/geometry/geometries/multi_point.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>




template <typename P>
void test_all()
{
    typedef bg::model::multi_point<P> mp;
    typedef bg::model::multi_linestring<bg::model::linestring<P> > ml;
    typedef bg::model::multi_polygon<bg::model::polygon<P> > mpoly;

    // All points below in upper-points and lower-points
    test_geometry<mp>("MULTIPOINT((0 0),(5 0),(1 1),(4 1))", 0, 5, 4.0);
    test_geometry<mp>("MULTIPOINT((0 1),(5 1),(1 0),(4 0))", 0, 5, 4.0);

    // All points in vertical line (this delivers an empty polygon with 3 points and closing point for closed)
    test_geometry<mp>("MULTIPOINT((1 0),(5 0),(3 0),(4 0),(2 0))", 0, 4, 0.0);

    // One point only
    test_geometry<mp>("MULTIPOINT((1 0))", 0, 4, 0.0);

    // Problem of 6019, reproduced by the convex hull robustness test:
    test_geometry<mp>("MULTIPOINT((2 9),(1 3),(9 4),(1 1),(1 0),(7 9),(2 5),(3 7),(3 6),(2 4))",
            0, 6, 48.0);

    // Ticket 6019:
    test_geometry<mp>("MULTIPOINT((0 53),(0 103),(0 53),(0 3),(0 3),(0 0),(1 0),(1 1),(2 1),(2 0),(2 0),(2 0),(3 0),(3 1),(4 1),(4 0),(5 0),(0 3),(10 3),(10 2),(10 2),(10 2),(5 2),(5 0),(5 0),(55 0),(105 0))",
            0, 4, 5407.5);
    // Ticket 6021:
    test_geometry<mp>("multipoint((0 53), (0 103), (1 53))", 3, 4, 25);

    test_geometry<mp>("multipoint((1.1 1.1), (2.5 2.1), (3.1 3.1), (4.9 1.1), (3.1 1.9))", 5, 4, 3.8);
    test_geometry<ml>("multilinestring((2 4, 3 4, 3 5), (4 3,4 4,5 4))", 6, 5, 3.0);
    test_geometry<mpoly>("multipolygon(((1 4,1 6,2 5,3 5,4 6,4 4,1 4)), ((4 2,4 3,6 3,6 2,4 2)))", 12, 7, 14.0);

    test_empty_input<mp>();
    test_empty_input<ml>();
    test_empty_input<mpoly>();
}


int test_main(int, char* [])
{
    //test_all<bg::model::d2::point_xy<int> >();
    //test_all<bg::model::d2::point_xy<float> >();
    test_all<bg::model::d2::point_xy<double> >();

#ifdef HAVE_TTMATH
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

    return 0;
}
