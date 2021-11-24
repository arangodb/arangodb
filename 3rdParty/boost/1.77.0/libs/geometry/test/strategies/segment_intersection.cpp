// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// Copyright (c) 2020, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#if defined(_MSC_VER)
// We deliberately mix float/double's here so turn off warning
#pragma warning( disable : 4244 )
#endif // defined(_MSC_VER)

#include <geometry_test_common.hpp>

#include <boost/geometry/algorithms/assign.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_as_subrange.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>

#include <boost/geometry/policies/relate/intersection_policy.hpp>

#include <boost/geometry/strategies/intersection_result.hpp>
#include <boost/geometry/strategies/relate/cartesian.hpp>


BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian);


template <typename P>
static void test_segment_intersection(int caseid,
                int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4,
                char expected_how,
                int expected_x1 = -99, int expected_y1 = -99,
                int expected_x2 = -99, int expected_y2 = -99)
{
    using namespace boost::geometry;

    typedef typename bg::coordinate_type<P>::type coordinate_type;
    typedef bg::model::referring_segment<const P> segment_type;

    P p1, p2, p3, p4;
    bg::assign_values(p1, x1, y1);
    bg::assign_values(p2, x2, y2);
    bg::assign_values(p3, x3, y3);
    bg::assign_values(p4, x4, y4);

    segment_type s12(p1,p2);
    segment_type s34(p3,p4);

    bg::detail::segment_as_subrange<segment_type> sr12(s12);
    bg::detail::segment_as_subrange<segment_type> sr34(s34);

    std::size_t expected_count = 0;

    if (expected_x1 != -99 && expected_y1 != -99)
    {
        expected_count++;
    }
    if (expected_x2 != -99 && expected_y2 != -99)
    {
        expected_count++;
    }

    // Using intersection_insert

    std::vector<P> out;
    bg::detail::intersection::intersection_insert<P>(s12, s34,
        std::back_inserter(out));

    // Using strategy
    typedef bg::segment_intersection_points<P> result_type;

    typedef bg::policies::relate::segments_intersection_points
        <
            result_type
        > points_policy_type;

    result_type is
        = bg::strategy::intersection::cartesian_segments<>
            ::apply(sr12, sr34, points_policy_type());

    bg::policies::relate::direction_type dir
        = bg::strategy::intersection::cartesian_segments<>
            ::apply(sr12, sr34, bg::policies::relate::segments_direction());

    //BOOST_CHECK_EQUAL(boost::size(out), expected_count);
    BOOST_CHECK_EQUAL(is.count, expected_count);
    BOOST_CHECK_MESSAGE(dir.how == expected_how,
            caseid
            << " how: detected: " << dir.how
            << " expected: "  << expected_how);

    if (expected_count == 2
            && is.count == 2
            && boost::size(out) == 2)
    {
        // Two intersection points, reverse expectation if necessary
        bool const first_matches
                = std::fabs(bg::get<0>(out[0]) - expected_x1) < 1.0e-6
               && std::fabs(bg::get<1>(out[0]) - expected_y1) < 1.0e-6;

        if (! first_matches)
        {
            std::swap(expected_x1, expected_x2);
            std::swap(expected_y1, expected_y2);
        }
    }

    if (expected_x1 != -99 && expected_y1 != -99
        && boost::size(out) >= 1)
    {

        BOOST_CHECK_CLOSE(bg::get<0>(out[0]), coordinate_type(expected_x1), 0.001);
        BOOST_CHECK_CLOSE(bg::get<1>(out[0]), coordinate_type(expected_y1), 0.001);

        BOOST_CHECK_CLOSE(bg::get<0>(is.intersections[0]), expected_x1, 0.001);
        BOOST_CHECK_CLOSE(bg::get<1>(is.intersections[0]), expected_y1, 0.001);

    }
    if (expected_x2 != -99 && expected_y2 != -99
        && boost::size(out) >= 2)
    {
        BOOST_CHECK_CLOSE(bg::get<0>(out[1]), coordinate_type(expected_x2), 0.001);
        BOOST_CHECK_CLOSE(bg::get<1>(out[1]), coordinate_type(expected_y2), 0.001);

        BOOST_CHECK_CLOSE(bg::get<0>(is.intersections[1]), expected_x2, 0.001);
        BOOST_CHECK_CLOSE(bg::get<1>(is.intersections[1]), expected_y2, 0.001);
    }
}


template <typename P>
void test_all()
{
    test_segment_intersection<P>( 1, 0,2, 2,0, 0,0, 2,2, 'i', 1, 1);
    test_segment_intersection<P>( 2, 2,2, 3,1, 0,0, 2,2, 'a', 2, 2);
    test_segment_intersection<P>( 3, 3,1, 2,2, 0,0, 2,2, 't', 2, 2);
    test_segment_intersection<P>( 4, 0,2, 1,1, 0,0, 2,2, 'm', 1, 1);

    test_segment_intersection<P>( 5, 1,1, 0,2, 0,0, 2,2, 's', 1, 1);
    test_segment_intersection<P>( 6, 0,2, 2,0, 0,0, 1,1, 'm', 1, 1);
    test_segment_intersection<P>( 7, 2,0, 0,2, 0,0, 1,1, 'm', 1, 1);
    test_segment_intersection<P>( 8, 2,3, 3,2, 0,0, 2,2, 'd');

    test_segment_intersection<P>( 9, 0,0, 2,2, 0,0, 2,2, 'e', 0, 0, 2, 2);
    test_segment_intersection<P>(10, 2,2, 0,0, 0,0, 2,2, 'e', 2, 2, 0, 0);
    test_segment_intersection<P>(11, 1,1, 3,3, 0,0, 2,2, 'c', 1, 1, 2, 2);
    test_segment_intersection<P>(12, 3,3, 1,1, 0,0, 2,2, 'c', 1, 1, 2, 2);

    test_segment_intersection<P>(13, 0,2, 2,2, 2,1, 2,3, 'm', 2, 2);
    test_segment_intersection<P>(14, 2,2, 2,4, 2,0, 2,2, 'a', 2, 2);
    test_segment_intersection<P>(15, 2,2, 2,4, 2,0, 2,1, 'd');
    test_segment_intersection<P>(16, 2,4, 2,2, 2,0, 2,1, 'd');

    test_segment_intersection<P>(17, 2,1, 2,3, 2,2, 2,4, 'c', 2, 2, 2, 3);
    test_segment_intersection<P>(18, 2,3, 2,1, 2,2, 2,4, 'c', 2, 3, 2, 2);
    test_segment_intersection<P>(19, 0,2, 2,2, 4,2, 2,2, 't', 2, 2);
    test_segment_intersection<P>(20, 0,2, 2,2, 2,2, 4,2, 'a', 2, 2);

    test_segment_intersection<P>(21, 1,2, 3,2, 2,1, 2,3, 'i', 2, 2);
    test_segment_intersection<P>(22, 2,4, 2,1, 2,1, 2,3, 'c', 2, 1, 2, 3);
    test_segment_intersection<P>(23, 2,4, 2,1, 2,3, 2,1, 'c', 2, 3, 2, 1);
    test_segment_intersection<P>(24, 1,1, 3,3, 0,0, 3,3, 'c', 1, 1, 3, 3);

    test_segment_intersection<P>(25, 2,0, 2,4, 2,1, 2,3, 'c', 2, 1, 2, 3);
    test_segment_intersection<P>(26, 2,0, 2,4, 2,3, 2,1, 'c', 2, 3, 2, 1);
    test_segment_intersection<P>(27, 0,0, 4,4, 1,1, 3,3, 'c', 1, 1, 3, 3);
    test_segment_intersection<P>(28, 0,0, 4,4, 3,3, 1,1, 'c', 3, 3, 1, 1);

    test_segment_intersection<P>(29, 1,1, 3,3, 0,0, 4,4, 'c', 1, 1, 3, 3);
    test_segment_intersection<P>(30, 0,0, 2,2, 2,2, 3,1, 'a', 2, 2);
    test_segment_intersection<P>(31, 0,0, 2,2, 2,2, 1,3, 'a', 2, 2);
    test_segment_intersection<P>(32, 0,0, 2,2, 1,1, 2,0, 's', 1, 1);

    test_segment_intersection<P>(33, 0,0, 2,2, 1,1, 0,2, 's', 1, 1);
    test_segment_intersection<P>(34, 2,2, 1,3, 0,0, 2,2, 'a', 2, 2);
    test_segment_intersection<P>(35, 2,2, 3,1, 0,0, 2,2, 'a', 2, 2);
    test_segment_intersection<P>(36, 0,0, 2,2, 0,2, 1,1, 'm', 1, 1);

    test_segment_intersection<P>(37, 0,0, 2,2, 2,0, 1,1, 'm', 1, 1);
    test_segment_intersection<P>(38, 1,1, 0,2, 0,0, 2,2, 's', 1, 1);
    test_segment_intersection<P>(39, 1,1, 2,0, 0,0, 2,2, 's', 1, 1);
    test_segment_intersection<P>(40, 2,0, 1,1, 0,0, 2,2, 'm', 1, 1);

    test_segment_intersection<P>(41, 1,2, 0,2, 2,2, 0,2, 'c', 1, 2, 0, 2);
    test_segment_intersection<P>(42, 2,1, 1,1, 2,2, 0,2, 'd');
    test_segment_intersection<P>(43, 4,1, 3,1, 2,2, 0,2, 'd');
    test_segment_intersection<P>(44, 4,2, 3,2, 2,2, 0,2, 'd');

    test_segment_intersection<P>(45, 2,0, 0,2, 0,0, 2,2, 'i', 1, 1);

    // In figure: times 2
    test_segment_intersection<P>(46, 8,2, 4,6, 0,0, 8, 8, 'i', 5, 5);
}

int test_main(int, char* [])
{
#if !defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<boost::tuple<double, double> >();
    test_all<bg::model::point<float, 2, bg::cs::cartesian> >();
#endif
    test_all<bg::model::point<double, 2, bg::cs::cartesian> >();

    return 0;
}
