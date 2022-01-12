// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

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
#include <boost/geometry/geometries/adapted/boost_variant.hpp>
#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/algorithms/is_valid.hpp>


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
    std::string const rectangle_without_holes = "polygon((1 1, 1 4, 4 4, 4 1, 1 1))";
    std::string const rectangle_with_holes = "polygon((1 1, 1 4, 4 4, 4 1, 1 1),(2 2, 3 2, 3 3, 2 3, 2 2))";

    using box_t = bg::model::box<P>;
    using ring_t = bg::model::ring<P>;
    using polygon_t = bg::model::polygon<P>;
    using mpolygon_t = bg::model::multi_polygon<polygon_t>;
    using variant_t = boost::variant<polygon_t, mpolygon_t>;
    using collection_t = bg::model::geometry_collection<variant_t>;

    test_one<ring_t>("triangle", triangle, true);
    test_one<ring_t>("concave1", concave1, false);
    test_one<bg::model::ring<P, false, false> >("triangle", triangle, true);
    test_one<bg::model::ring<P, false, false> >("concave1", concave1, false);

    test_one<polygon_t>("triangle", triangle, true);
    test_one<polygon_t>("concave1", concave1, false);
    test_one<polygon_t>("rectangle_without_holes", rectangle_without_holes, true);
    test_one<polygon_t>("rectangle_with_holes", rectangle_with_holes, false);

    test_one<box_t>("box", "box(0 0,2 2)", true);

    test_one<mpolygon_t>("mpoly1", "multipolygon(((1 1, 1 4, 5 1, 1 1)))", true);
    test_one<mpolygon_t>("mpoly1", "multipolygon(((1 1, 1 4, 3 4, 3 3, 4 3, 4 4, 5 4, 5 1, 1 1)))", false);
    test_one<mpolygon_t>("mpoly2", "multipolygon(((1 1, 1 4, 5 1, 1 1)),((3 0, 3 1, 4 0, 3 0)))", false);

    test_one<variant_t>("variant1", triangle, true);
    test_one<variant_t>("variant2", concave1, false);

    std::string const pref = "geometrycollection(";
    std::string const post = ")";
    test_one<collection_t>("collection1", pref + triangle + post, true);
    test_one<collection_t>("collection2", pref + concave1 + post, false);
    test_one<collection_t>("collection3", pref + triangle + ", " + concave1 + post, false);
    test_one<collection_t>("collection4", pref + triangle + ", polygon((3 0, 3 1, 4 0, 3 0))" + post, false);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<int> >();
    test_all<bg::model::d2::point_xy<double> >();

    return 0;
}
