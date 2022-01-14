// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <algorithms/test_length.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

template <typename P>
void test_all()
{
    test_geometry<bg::model::multi_linestring<bg::model::linestring<P> > >
        ("MULTILINESTRING((0 0,3 4,4 3))", 5 + sqrt(2.0));
}

template <typename P>
void test_empty_input()
{
    test_empty_input(bg::model::multi_linestring<P>());
}

int test_main( int , char* [] )
{
    test_all<bg::model::d2::point_xy<double> >();

    // test_empty_input<bg::model::d2::point_xy<int> >();

    return 0;
}
