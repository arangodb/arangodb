// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_GEOMETRY_DEFINE_STREAM_OPERATOR_SEGMENT_RATIO

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/assign.hpp>

#include <boost/geometry/strategies/cartesian/intersection.hpp>
#include <boost/geometry/strategies/intersection_result.hpp>

#include <boost/geometry/policies/relate/intersection_points.hpp>
#include <boost/geometry/policies/relate/direction.hpp>
#include <boost/geometry/policies/relate/tupled.hpp>

#include <boost/geometry/algorithms/intersection.hpp>


#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/segment.hpp>


template <typename IntersectionPoints>
static int check(IntersectionPoints const& is,
                std::size_t index, double expected_x, double expected_y)
{
    if (expected_x != -99 && expected_y != -99 && is.count > index)
    {
        double x = bg::get<0>(is.intersections[index]);
        double y = bg::get<1>(is.intersections[index]);

        BOOST_CHECK_CLOSE(x, expected_x, 0.001);
        BOOST_CHECK_CLOSE(y, expected_y, 0.001);
        return 1;
    }
    return 0;
}


template <typename P>
static void test_segment_intersection(std::string const& case_id,
                int x1, int y1, int x2, int y2,
                int x3, int y3, int x4, int y4,
                char expected_how, bool expected_opposite,
                int expected_arrival1, int expected_arrival2,
                int expected_x1, int expected_y1,
                int expected_x2 = -99, int expected_y2 = -99)

{
    boost::ignore_unused(case_id);

    typedef bg::model::referring_segment<const P> segment_type;

    P p1, p2, p3, p4;
    bg::assign_values(p1, x1, y1);
    bg::assign_values(p2, x2, y2);
    bg::assign_values(p3, x3, y3);
    bg::assign_values(p4, x4, y4);

    segment_type s12(p1,p2);
    segment_type s34(p3,p4);

    typedef bg::detail::no_rescale_policy rescale_policy_type;
    rescale_policy_type rescale_policy;

    typedef bg::segment_intersection_points
    <
        P,
        typename bg::segment_ratio_type
        <
            P,
            rescale_policy_type
        >::type
    > result_type;

    typedef bg::policies::relate::segments_intersection_points
        <
            result_type
        > points_policy_type;

    // Get the intersection point (or two points)
    result_type is
        = bg::strategy::intersection::cartesian_segments<>
            ::apply(s12, s34, points_policy_type(), rescale_policy, p1, p2, p3, p4);

    // Get just a character for Left/Right/intersects/etc, purpose is more for debugging
    bg::policies::relate::direction_type dir
        = bg::strategy::intersection::cartesian_segments<>
            ::apply(s12, s34, bg::policies::relate::segments_direction(),
                    rescale_policy, p1, p2, p3, p4);

    std::size_t expected_count =
        check(is, 0, expected_x1, expected_y1)
        + check(is, 1, expected_x2, expected_y2);

    BOOST_CHECK_EQUAL(is.count, expected_count);
    BOOST_CHECK_EQUAL(dir.how, expected_how);
    BOOST_CHECK_EQUAL(dir.opposite, expected_opposite);
    BOOST_CHECK_EQUAL(dir.arrival[0], expected_arrival1);
    BOOST_CHECK_EQUAL(dir.arrival[1], expected_arrival2);
}

template <typename P, typename Pair>
static void test_segment_ratio(std::string const& case_id,
                int x1, int y1, int x2, int y2,
                int x3, int y3, int x4, int y4,
                Pair expected_pair_a1, Pair expected_pair_a2,
                Pair expected_pair_b1, Pair expected_pair_b2,
                int exp_ax1, int exp_ay1, int exp_ax2, int exp_ay2,
                std::size_t expected_count = 2)

{
    boost::ignore_unused(case_id);

    typedef bg::model::referring_segment<const P> segment_type;

    P p1, p2, p3, p4;
    bg::assign_values(p1, x1, y1);
    bg::assign_values(p2, x2, y2);
    bg::assign_values(p3, x3, y3);
    bg::assign_values(p4, x4, y4);

    segment_type s12(p1, p2);
    segment_type s34(p3, p4);

    typedef bg::detail::no_rescale_policy rescale_policy_type;
    rescale_policy_type rescale_policy;

    typedef typename bg::segment_ratio_type<P, rescale_policy_type>::type ratio_type;
    typedef bg::segment_intersection_points
    <
        P,
        ratio_type
    > result_type;

    typedef bg::policies::relate::segments_intersection_points
        <
            result_type
        > points_policy_type;

    // Get the intersection point (or two points)
    result_type is
        = bg::strategy::intersection::cartesian_segments<>
            ::apply(s12, s34, points_policy_type(), rescale_policy, p1, p2, p3, p4);

    ratio_type expected_a1(expected_pair_a1.first, expected_pair_a1.second);
    ratio_type expected_a2(expected_pair_a2.first, expected_pair_a2.second);
    ratio_type expected_b1(expected_pair_b1.first, expected_pair_b1.second);
    ratio_type expected_b2(expected_pair_b2.first, expected_pair_b2.second);

    BOOST_CHECK_EQUAL(is.count, expected_count);

    BOOST_CHECK_EQUAL(is.fractions[0].robust_ra, expected_a1);
    BOOST_CHECK_EQUAL(is.fractions[0].robust_rb, expected_b1);
    BOOST_CHECK_EQUAL(bg::get<0>(is.intersections[0]), exp_ax1);
    BOOST_CHECK_EQUAL(bg::get<1>(is.intersections[0]), exp_ay1);

    if (expected_count == 2)
    {
        BOOST_CHECK_EQUAL(bg::get<0>(is.intersections[1]), exp_ax2);
        BOOST_CHECK_EQUAL(bg::get<1>(is.intersections[1]), exp_ay2);
        BOOST_CHECK_EQUAL(is.fractions[1].robust_ra, expected_a2);
        BOOST_CHECK_EQUAL(is.fractions[1].robust_rb, expected_b2);
    }
}


template <typename P>
void test_all()
{
    // Collinear - non opposite

    //       a1---------->a2
    // b1--->b2
    test_segment_intersection<P>("n1",
        2, 0, 6, 0,
        0, 0, 2, 0,
        'a', false, -1, 0,
        2, 0);

    //       a1---------->a2
    //    b1--->b2
    test_segment_intersection<P>("n2",
        2, 0, 6, 0,
        1, 0, 3, 0,
        'c', false, -1, 1,
        2, 0, 3, 0);

    //       a1---------->a2
    //       b1--->b2
    test_segment_intersection<P>("n3",
        2, 0, 6, 0,
        2, 0, 4, 0,
        'c', false, -1, 1,
        2, 0, 4, 0);

    //       a1---------->a2
    //          b1--->b2
    test_segment_intersection<P>("n4",
        2, 0, 6, 0,
        3, 0, 5, 0,
        'c', false, -1, 1,
        3, 0, 5, 0);

    //       a1---------->a2
    //              b1--->b2
    test_segment_intersection<P>("n5",
        2, 0, 6, 0,
        4, 0, 6, 0,
        'c', false, 0, 0,
        4, 0, 6, 0);

    //       a1---------->a2
    //                 b1--->b2
    test_segment_intersection<P>("n6",
        2, 0, 6, 0,
        5, 0, 7, 0,
        'c', false, 1, -1,
        5, 0, 6, 0);

    //       a1---------->a2
    //                    b1--->b2
    test_segment_intersection<P>("n7",
        2, 0, 6, 0,
        6, 0, 8, 0,
        'a', false, 0, -1,
        6, 0);

    // Collinear - opposite
    //       a1---------->a2
    // b2<---b1
    test_segment_intersection<P>("o1",
        2, 0, 6, 0,
        2, 0, 0, 0,
        'f', true, -1, -1,
        2, 0);

    //       a1---------->a2
    //    b2<---b1
    test_segment_intersection<P>("o2",
        2, 0, 6, 0,
        3, 0, 1, 0,
        'c', true, -1, -1,
        2, 0, 3, 0);

    //       a1---------->a2
    //       b2<---b1
    test_segment_intersection<P>("o3",
        2, 0, 6, 0,
        4, 0, 2, 0,
        'c', true, -1, 0,
        2, 0, 4, 0);

    //       a1---------->a2
    //           b2<---b1
    test_segment_intersection<P>("o4",
        2, 0, 6, 0,
        5, 0, 3, 0,
        'c', true, -1, 1,
        3, 0, 5, 0);

    //       a1---------->a2
    //              b2<---b1
    test_segment_intersection<P>("o5",
        2, 0, 6, 0,
        6, 0, 4, 0,
        'c', true, 0, 1,
        4, 0, 6, 0);

    //       a1---------->a2
    //                b2<---b1
    test_segment_intersection<P>("o6",
        2, 0, 6, 0,
        7, 0, 5, 0,
        'c', true, 1, 1,
        5, 0, 6, 0);

    //       a1---------->a2
    //                    b2<---b1
    test_segment_intersection<P>("o7",
        2, 0, 6, 0,
        8, 0, 6, 0,
        't', true, 0, 0,
        6, 0);

    //   a1---------->a2
    //   b1---------->b2
    test_segment_intersection<P>("e1",
        2, 0, 6, 0,
        2, 0, 6, 0,
        'e', false, 0, 0,
        2, 0, 6, 0);

    //   a1---------->a2
    //   b2<----------b1
    test_segment_intersection<P>("e1",
        2, 0, 6, 0,
        6, 0, 2, 0,
        'e', true, 0, 0,
        2, 0, 6, 0);

    // Disjoint (in vertical direction, picture still horizontal)
    //   a2<---a1
    //                      b2<---b1
    test_segment_intersection<P>("case_recursive_boxes_1",
        10, 7, 10, 6,
        10, 10, 10, 9,
        'd', false, 0, 0,
        -1, -1, -1, -1);

}


template <typename P>
void test_ratios()
{
    // B inside A
    //       a1------------>a2
    //          b1--->b2
    test_segment_ratio<P>("n4",
        2, 0, 7, 0,
        3, 0, 5, 0,
        std::make_pair(1, 5), std::make_pair(3, 5), // IP located on 1/5, 3/5 w.r.t A
        std::make_pair(0, 1), std::make_pair(1, 1), // IP located on 0, 1 w.r.t. B
        // IP's are ordered as in A (currently)
        3, 0, 5, 0);

    //       a1------------>a2
    //           b2<---b1
    test_segment_ratio<P>("o4",
        2, 0, 7, 0,
        5, 0, 3, 0,
        std::make_pair(1, 5), std::make_pair(3, 5),
        std::make_pair(1, 1), std::make_pair(0, 1),
        3, 0, 5, 0);

    //       a2<------------a1
    //           b2<---b1
    test_segment_ratio<P>("o4b",
        7, 0, 2, 0,
        5, 0, 3, 0,
        std::make_pair(2, 5), std::make_pair(4, 5),
        std::make_pair(0, 1), std::make_pair(1, 1),
        5, 0, 3, 0);

    //       a2<------------a1
    //          b1--->b2
    test_segment_ratio<P>("o4c",
        7, 0, 2, 0,
        3, 0, 5, 0,
        std::make_pair(2, 5), std::make_pair(4, 5),
        std::make_pair(1, 1), std::make_pair(0, 1),
        5, 0, 3, 0);

    // Touch-interior
    //       a1---------->a2
    //       b1--->b2
    test_segment_ratio<P>("n3",
        2, 0, 7, 0,
        2, 0, 4, 0,
        std::make_pair(0, 1), std::make_pair(2, 5),
        std::make_pair(0, 1), std::make_pair(1, 1),
        2, 0, 4, 0);

    //    a2<-------------a1
    //             b2<----b1
    test_segment_ratio<P>("n3b",
        7, 0, 2, 0,
        7, 0, 5, 0,
        std::make_pair(0, 1), std::make_pair(2, 5),
        std::make_pair(0, 1), std::make_pair(1, 1),
        7, 0, 5, 0);


    // A inside B
    //          a1--->a2
    //       b1------------>b2
    test_segment_ratio<P>("rn4",
        3, 0, 5, 0,
        2, 0, 7, 0,
        std::make_pair(0, 1), std::make_pair(1, 1),
        std::make_pair(1, 5), std::make_pair(3, 5),
        3, 0, 5, 0);

    //           a2<---a1
    //       b1------------>b2
    test_segment_ratio<P>("ro4",
        5, 0, 3, 0,
        2, 0, 7, 0,
        std::make_pair(0, 1), std::make_pair(1, 1),
        std::make_pair(3, 5), std::make_pair(1, 5),
        5, 0, 3, 0);

    //           a2<---a1
    //       b2<------------b1
    test_segment_ratio<P>("ro4b",
        5, 0, 3, 0,
        7, 0, 2, 0,
        std::make_pair(0, 1), std::make_pair(1, 1),
        std::make_pair(2, 5), std::make_pair(4, 5),
        5, 0, 3, 0);

    //          a1--->a2
    //       b2<------------b1
    test_segment_ratio<P>("ro4c",
        3, 0, 5, 0,
        7, 0, 2, 0,
        std::make_pair(0, 1), std::make_pair(1, 1),
        std::make_pair(4, 5), std::make_pair(2, 5),
        3, 0, 5, 0);

    // B inside A, boundaries intersect
    // We change the coordinates a bit (w.r.t. n3 above) to have it asymmetrical
    //       a1---------->a2
    //       b1--->b2
    test_segment_ratio<P>("n3",
        2, 0, 7, 0,
        2, 0, 4, 0,
        std::make_pair(0, 1), std::make_pair(2, 5),
        std::make_pair(0, 1), std::make_pair(1, 1),
        2, 0, 4, 0);

    //       a1---------->a2
    //       b2<---b1
    test_segment_ratio<P>("o3",
        2, 0, 7, 0,
        4, 0, 2, 0,
        std::make_pair(0, 1), std::make_pair(2, 5),
        std::make_pair(1, 1), std::make_pair(0, 1),
        2, 0, 4, 0);

    //       a1---------->a2
    //              b1--->b2
    test_segment_ratio<P>("n5",
        2, 0, 7, 0,
        5, 0, 7, 0,
        std::make_pair(3, 5), std::make_pair(1, 1),
        std::make_pair(0, 1), std::make_pair(1, 1),
        5, 0, 7, 0);

    //       a1---------->a2
    //              b2<---b1
    test_segment_ratio<P>("o5",
        2, 0, 7, 0,
        7, 0, 5, 0,
        std::make_pair(3, 5), std::make_pair(1, 1),
        std::make_pair(1, 1), std::make_pair(0, 1),
        5, 0, 7, 0);

    // Generic (overlaps)
    //       a1---------->a2
    //    b1----->b2
    test_segment_ratio<P>("n2",
        2, 0, 7, 0,
        1, 0, 4, 0,
        std::make_pair(0, 1), std::make_pair(2, 5),
        std::make_pair(1, 3), std::make_pair(1, 1),
        2, 0, 4, 0);

    // Same, b reversed
    test_segment_ratio<P>("n2_b",
        2, 0, 7, 0,
        4, 0, 1, 0,
        std::make_pair(0, 1), std::make_pair(2, 5),
        std::make_pair(2, 3), std::make_pair(0, 1),
        2, 0, 4, 0);

    // Same, both reversed
    test_segment_ratio<P>("n2_c",
        7, 0, 2, 0,
        4, 0, 1, 0,
        std::make_pair(3, 5), std::make_pair(1, 1),
        std::make_pair(0, 1), std::make_pair(2, 3),
        4, 0, 2, 0);

    //       a1---------->a2
    //                 b1----->b2
    test_segment_ratio<P>("n6",
        2, 0, 6, 0,
        5, 0, 8, 0,
        std::make_pair(3, 4), std::make_pair(1, 1),
        std::make_pair(0, 1), std::make_pair(1, 3),
        5, 0, 6, 0);

    // Degenerated one
    //       a1---------->a2
    //             b1/b2
    const int ignored = 99;
    test_segment_ratio<P>("degenerated1",
        2, 0, 6, 0,
        5, 0, 5, 0,
        std::make_pair(3, 4), // IP located on 3/4 w.r.t A
        std::make_pair(ignored, 1), // not checked
        std::make_pair(0, 1), // IP located at any place w.r.t B, so 0
        std::make_pair(ignored, 1), // not checked
        5, 0,
        ignored, ignored,
        1);

    test_segment_ratio<P>("degenerated2",
        5, 0, 5, 0,
        2, 0, 6, 0,
        std::make_pair(0, 1), std::make_pair(ignored, 1),
        std::make_pair(3, 4), std::make_pair(ignored, 1),
        5, 0,
        ignored, ignored,
        1);

    // Vertical one like in box_poly5 but in integer
    test_segment_ratio<P>("box_poly5",
        45, 25, 45, 15,
        45, 22, 45, 19,
        std::make_pair(3, 10), std::make_pair(6, 10),
        std::make_pair(0, 1), std::make_pair(1, 1),
        45, 22, 45, 19);
}

int test_main(int, char* [])
{
    // We don't rescale but use integer points as, by nature, robust points
    test_all<bg::model::point<int, 2, bg::cs::cartesian> >();
    test_ratios<bg::model::point<int, 2, bg::cs::cartesian> >();
    return 0;
}
