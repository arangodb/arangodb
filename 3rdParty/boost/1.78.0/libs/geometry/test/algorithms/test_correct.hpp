// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

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
#include <boost/geometry/geometries/adapted/boost_variant.hpp>
#include <boost/geometry/geometries/geometry_collection.hpp>
#include <boost/geometry/io/wkt/read.hpp>
#include <boost/geometry/io/wkt/write.hpp>

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

    bg::correct(geometry);
    check_geometry(geometry, expected);

    boost::variant<Geometry> v(geometry);
    
    bg::correct(v);
    check_geometry(v, expected);

    std::string const pref = "GEOMETRYCOLLECTION(";
    std::string const post = ")";
    bg::model::geometry_collection<boost::variant<Geometry>> gc = { v };

    bg::correct(gc);
    check_geometry(gc, pref + expected + post);
}


#endif
