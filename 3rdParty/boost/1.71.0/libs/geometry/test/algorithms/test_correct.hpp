// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_CORRECT_HPP
#define BOOST_GEOMETRY_TEST_CORRECT_HPP


// Test-functionality, shared between single and multi tests

#include <sstream>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>
#include <boost/variant/variant.hpp>

template <typename Geometry>
void check_geometry(Geometry const& geometry, std::string const& expected)
{
    std::ostringstream out;
    out << bg::wkt_manipulator<Geometry>(geometry, false);

    BOOST_CHECK_EQUAL(out.str(), expected);
}

template <typename Geometry>
void test_geometry(std::string const& wkt, std::string const& expected)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    boost::variant<Geometry> v(geometry);

    bg::correct(geometry);
    check_geometry(geometry, expected);

    bg::correct(v);
    check_geometry(v, expected);
}


#endif
