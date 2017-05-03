// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/radius.hpp>
#include <boost/geometry/core/srs.hpp>

#include <boost/geometry/algorithms/make.hpp>


template <std::size_t I, typename G>
void test_get_set()
{
    typedef typename bg::radius_type<G>::type radius_type;

    G g;
    bg::set_radius<I>(g, radius_type(5));

    radius_type x = bg::get_radius<I>(g);

    BOOST_CHECK_CLOSE(double(x), 5.0, 0.0001);
}

template <typename T>
void test_all()
{
    typedef bg::srs::spheroid<T> Sd;
    test_get_set<0, Sd>();
    test_get_set<2, Sd>();

    typedef bg::srs::sphere<T> Se;
    test_get_set<0, Se>();
}


int test_main(int, char* [])
{
    test_all<int>();
    test_all<float>();
    test_all<double>();

    return 0;
}
