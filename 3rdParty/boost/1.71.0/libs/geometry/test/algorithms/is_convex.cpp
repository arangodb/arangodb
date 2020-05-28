// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <string>

#include <boost/geometry/algorithms/is_convex.hpp>

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/ring.hpp>


template <typename Geometry>
void test_one(std::string const& case_id, std::string const& wkt, bool expected)
{
    Geometry geometry;
    bg::read_wkt(wkt, geometry);
    bg::correct(geometry);

    bool detected = bg::is_convex(geometry);
    BOOST_CHECK_MESSAGE(detected == expected,
        "Not as expected, case: " << case_id
            << " / expected: " << expected
            << " / detected: " << detected);
}


template <typename P>
void test_all()
{
    // rectangular, with concavity
    std::string const concave1 = "polygon((1 1, 1 4, 3 4, 3 3, 4 3, 4 4, 5 4, 5 1, 1 1))";
    std::string const triangle = "polygon((1 1, 1 4, 5 1, 1 1))";

    test_one<bg::model::ring<P> >("triangle", triangle, true);
    test_one<bg::model::ring<P> >("concave1", concave1, false);
    test_one<bg::model::ring<P, false, false> >("triangle", triangle, true);
    test_one<bg::model::ring<P, false, false> >("concave1", concave1, false);

    test_one<bg::model::box<P> >("box", "box(0 0,2 2)", true);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
