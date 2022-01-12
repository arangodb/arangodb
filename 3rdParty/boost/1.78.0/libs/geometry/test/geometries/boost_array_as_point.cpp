// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010 Alfredo Correa
// Copyright (c) 2010-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/geometries/adapted/boost_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/std_array.hpp>
#include <boost/geometry/geometries/point.hpp>

BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_STD_ARRAY_CS(cs::cartesian)

int test_main(int, char* [])
{
    bg::model::point<double, 3, bg::cs::cartesian> p1(1,2,3);
    double p2[3] = {4,5,6};
    boost::tuple<double, double, double> p3(7,8,9);
    boost::array<double, 3> p4 = {{10,11,12}};
    std::array<double, 3> p5 = {{13,14,15}};

    std::clog << bg::distance(p1, p2) << std::endl;
    std::clog << bg::distance(p2, p3) << std::endl;
    std::clog << bg::distance(p3, p4) << std::endl;
    std::clog << bg::distance(p4, p5) << std::endl;

    return 0;
}

