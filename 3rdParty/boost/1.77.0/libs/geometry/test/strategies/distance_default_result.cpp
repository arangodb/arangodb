// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_distance_default_result
#endif

#include <cstddef>
#include <iostream>

#include <boost/test/included/unit_test.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/util/calculation_type.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/geometries/box.hpp>

#include <boost/geometry/strategies/strategies.hpp>
#include <boost/geometry/strategies/default_distance_result.hpp>
#include <boost/geometry/strategies/default_comparable_distance_result.hpp>

namespace bg = ::boost::geometry;


template <typename DefaultResult, typename ExpectedResult>
struct assert_equal_types
{
    assert_equal_types()
    {
        static const bool are_same =
            boost::is_same<DefaultResult, ExpectedResult>::type::value;

        BOOST_MPL_ASSERT_MSG((are_same),
                             WRONG_DEFAULT_DISTANCE_RESULT,
                             (types<DefaultResult, ExpectedResult>));
    }
};

//=========================================================================

template
<
    typename Geometry1,
    typename Geometry2,
    typename ExpectedResult,
    typename ExpectedComparableResult
>
inline void test_distance_result()
{
    typedef typename bg::default_distance_result
        <
            Geometry1, Geometry2
        >::type result12;

    typedef typename bg::default_distance_result
        <
            Geometry2, Geometry1
        >::type result21;

    typedef typename bg::default_comparable_distance_result
        <
            Geometry1, Geometry2
        >::type comparable_result12;

    typedef typename bg::default_comparable_distance_result
        <
            Geometry2, Geometry1
        >::type comparable_result21;

    assert_equal_types<result12, ExpectedResult>();
    assert_equal_types<result21, ExpectedResult>();
    assert_equal_types<comparable_result12, ExpectedComparableResult>();
    assert_equal_types<comparable_result21, ExpectedComparableResult>();
}

//=========================================================================

template
<
    typename CoordinateType1,
    typename CoordinateType2,
    std::size_t Dimension,
    typename CoordinateSystem,
    typename ExpectedResult,
    typename ExpectedComparableResult = ExpectedResult
>
struct test_distance_result_segment
{
    test_distance_result_segment()
    {
        typedef typename bg::model::point
            <
                CoordinateType1, Dimension, CoordinateSystem
            > point1;

        typedef typename bg::model::point
            <
                CoordinateType2, Dimension, CoordinateSystem
            > point2;

        typedef typename bg::model::segment<point1> segment1;
        typedef typename bg::model::segment<point2> segment2;

        test_distance_result
            <
                point1, point2, ExpectedResult, ExpectedComparableResult
            >();

        test_distance_result
            <
                point1, segment2, ExpectedResult, ExpectedComparableResult
            >();

        test_distance_result
            <
                point2, segment1, ExpectedResult, ExpectedComparableResult
            >();
    }
};

//=========================================================================

template
<
    typename CoordinateType1,
    typename CoordinateType2,
    std::size_t Dimension,
    typename ExpectedResult,
    typename ExpectedComparableResult = ExpectedResult
>
struct test_distance_result_box
{
    test_distance_result_box()
    {
        typedef typename bg::model::point
            <
                CoordinateType1, Dimension, bg::cs::cartesian
            > point1;

        typedef typename bg::model::point
            <
                CoordinateType2, Dimension, bg::cs::cartesian
            > point2;

        typedef typename bg::model::box<point1> box1;
        typedef typename bg::model::box<point2> box2;

        test_distance_result
            <
                point1, box2, ExpectedResult, ExpectedComparableResult
            >();

        test_distance_result
            <
                point2, box1, ExpectedResult, ExpectedComparableResult
            >();

        test_distance_result
            <
                box1, box2, ExpectedResult, ExpectedComparableResult
            >();
    }
};

//=========================================================================

template <std::size_t D, typename CoordinateSystem>
inline void test_segment_all()
{
    typedef typename boost::mpl::if_
        <
            typename boost::is_same<CoordinateSystem, bg::cs::cartesian>::type,
            double,
            float
        >::type float_return_type;

    test_distance_result_segment<short, short, D, CoordinateSystem, double>();
    test_distance_result_segment<int, int, D, CoordinateSystem, double>();
    test_distance_result_segment<int, long, D, CoordinateSystem, double>();
    test_distance_result_segment<long, long, D, CoordinateSystem, double>();

    test_distance_result_segment<int, float, D, CoordinateSystem, float_return_type>();
    test_distance_result_segment<float, float, D, CoordinateSystem, float_return_type>();

    test_distance_result_segment<int, double, D, CoordinateSystem, double>();
    test_distance_result_segment<double, int, D, CoordinateSystem, double>();
    test_distance_result_segment<float, double, D, CoordinateSystem, double>();
    test_distance_result_segment<double, float, D, CoordinateSystem, double>();
    test_distance_result_segment<double, double, D, CoordinateSystem, double>();
}

//=========================================================================

template <std::size_t D>
inline void test_box_all()
{
    typedef bg::util::detail::default_integral::type default_integral;

    test_distance_result_box<short, short, D, double, default_integral>();
    test_distance_result_box<int, int, D, double, default_integral>();
    test_distance_result_box<int, long, D, double, default_integral>();
    test_distance_result_box<long, long, D, double, default_integral>();

    test_distance_result_box<int, float, D, double>();
    test_distance_result_box<float, float, D, double>();

    test_distance_result_box<int, double, D, double>();
    test_distance_result_box<double, int, D, double>();
    test_distance_result_box<float, double, D, double>();
    test_distance_result_box<double, float, D, double>();
    test_distance_result_box<double, double, D, double>();
}

//=========================================================================

BOOST_AUTO_TEST_CASE( test_point_point_or_point_segment )
{
    test_segment_all<2, bg::cs::cartesian>();
    test_segment_all<3, bg::cs::cartesian>();
    test_segment_all<4, bg::cs::cartesian>();
    test_segment_all<2, bg::cs::spherical_equatorial<bg::degree> >();
}

BOOST_AUTO_TEST_CASE( test_point_box_or_box )
{
    test_box_all<2>();
    test_box_all<3>();
    test_box_all<4>();
}
