// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2014-2020.
// Modifications copyright (c) 2014-2020 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <strategies/test_within.hpp>


template <typename Point>
void test_box_of(std::string const& wkt_point, std::string const& wkt_box,
              bool expected_within, bool expected_covered_by)
{
    typedef bg::model::box<Point> box_type;
    
    Point point;
    box_type box;
    bg::read_wkt(wkt_point, point);
    bg::read_wkt(wkt_box, box);

    bool detected_within = bg::within(point, box);
    bool detected_covered_by = bg::covered_by(point, box);
    BOOST_CHECK_EQUAL(detected_within, expected_within);
    BOOST_CHECK_EQUAL(detected_covered_by, expected_covered_by);

    // Also test with the non-default side version
    bg::strategy::within::cartesian_point_box_by_side<> within_strategy;
    bg::strategy::covered_by::cartesian_point_box_by_side<> covered_by_strategy;

    detected_within = bg::within(point, box, within_strategy);
    detected_covered_by = bg::covered_by(point, box, covered_by_strategy);
    BOOST_CHECK_EQUAL(detected_within, expected_within);
    BOOST_CHECK_EQUAL(detected_covered_by, expected_covered_by);

    // BREAKING CHANGE:
    // Internally the strategies are converted to umbrella strategy
    // and then the umbrella strategy is used to get correct strategy
    // for corresponding algorithm.
    // PREVIOUSLY:
    // We might exchange strategies between within/covered by.
    // So the lines below might seem confusing, but are as intended
    //detected_within = bg::covered_by(point, box, within_strategy);
    //detected_covered_by = bg::within(point, box, covered_by_strategy);
    //BOOST_CHECK_EQUAL(detected_within, expected_within);
    //BOOST_CHECK_EQUAL(detected_covered_by, expected_covered_by);

    // Finally we call the strategies directly
    detected_within = within_strategy.apply(point, box);
    detected_covered_by = covered_by_strategy.apply(point, box);
    BOOST_CHECK_EQUAL(detected_within, expected_within);
    BOOST_CHECK_EQUAL(detected_covered_by, expected_covered_by);
}

template <typename Point>
void test_box()
{
    test_box_of<Point>("POINT(1 1)", "BOX(0 0,2 2)", true, true);
    test_box_of<Point>("POINT(0 0)", "BOX(0 0,2 2)", false, true);
    test_box_of<Point>("POINT(2 2)", "BOX(0 0,2 2)", false, true);
    test_box_of<Point>("POINT(0 1)", "BOX(0 0,2 2)", false, true);
    test_box_of<Point>("POINT(1 0)", "BOX(0 0,2 2)", false, true);
    test_box_of<Point>("POINT(3 3)", "BOX(0 0,2 2)", false, false);
}


int test_main(int, char* [])
{
    test_box<bg::model::point<float, 2, bg::cs::cartesian> >();
    test_box<bg::model::point<double, 2, bg::cs::cartesian> >();

    return 0;
}
