// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2019 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2016.
// Modifications copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <boost/variant/variant.hpp>

#include "geometry_test_common.hpp"

#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/core/coordinate_type.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include "test_common/test_point.hpp"


template <typename P>
void test_all()
{
    typedef typename bg::coordinate_type<P>::type coordinate_type;

    P p1(0, 0);
    P p2(2, 2);

    typedef bg::model::box<P> box_type;

    box_type b1(p1, p2);
    box_type b2;
    bg::buffer(b1, b2, coordinate_type(2));

    box_type expected(P(-2, -2), P(4, 4));
    BOOST_CHECK(bg::equals(b2, expected));

    boost::variant<box_type> v(b1);
    bg::buffer(v, b2, coordinate_type(2));

    BOOST_CHECK(bg::equals(b2, expected));
}

int test_main(int, char* [])
{
    test_all<bg::model::point<int, 2, bg::cs::cartesian> >();
    test_all<bg::model::point<float, 2, bg::cs::cartesian> >();
    test_all<bg::model::point<double, 2, bg::cs::cartesian> >();

#ifdef HAVE_TTMATH
    test_all<bg::model::point<ttmath_big, 2, bg::cs::cartesian> >();
#endif
    return 0;
}
