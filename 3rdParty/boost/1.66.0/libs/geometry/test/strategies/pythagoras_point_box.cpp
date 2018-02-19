// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2007-2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2014 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2014 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2014.
// Modifications copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_pythagoras_point_box
#endif

#include <boost/test/included/unit_test.hpp>

#if defined(_MSC_VER)
#  pragma warning( disable : 4101 )
#endif

#include <boost/core/ignore_unused.hpp>
#include <boost/timer.hpp>

#include <boost/concept/requires.hpp>
#include <boost/concept_check.hpp>

#include <boost/geometry/algorithms/assign.hpp>
#include <boost/geometry/algorithms/expand.hpp>
#include <boost/geometry/strategies/cartesian/distance_pythagoras_point_box.hpp>
#include <boost/geometry/strategies/concepts/distance_concept.hpp>


#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/adapted/c_array.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>

#include <test_common/test_point.hpp>

#ifdef HAVE_TTMATH
#  include <boost/geometry/extensions/contrib/ttmath_stub.hpp>
#endif


namespace bg = boost::geometry;


BOOST_GEOMETRY_REGISTER_C_ARRAY_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)


template <typename Box, typename Coordinate>
inline void assign_values(Box& box,
                          Coordinate const& x1,
                          Coordinate const& y1,
                          Coordinate const& z1,
                          Coordinate const& x2,
                          Coordinate const& y2,
                          Coordinate const& z2)
{
    typename bg::point_type<Box>::type p1, p2;
    bg::assign_values(p1, x1, y1, z1);
    bg::assign_values(p2, x2, y2, z2);
    bg::assign(box, p1);
    bg::expand(box, p2);
}

template <typename Point, typename Box>
inline void test_null_distance_3d()
{
    Point p;
    bg::assign_values(p, 1, 2, 4);
    Box b;
    assign_values(b, 1, 2, 3, 4, 5, 6);

    typedef bg::strategy::distance::pythagoras_point_box<> pythagoras_pb_type;
    typedef typename bg::strategy::distance::services::return_type
        <
            pythagoras_pb_type, Point, Box
        >::type return_type;

    pythagoras_pb_type pythagoras_pb;
    return_type result = pythagoras_pb.apply(p, b);

    BOOST_CHECK_EQUAL(result, return_type(0));

    bg::assign_values(p, 1, 3, 4);
    result = pythagoras_pb.apply(p, b);
    BOOST_CHECK_EQUAL(result, return_type(0));

    bg::assign_values(p, 2, 3, 4);
    result = pythagoras_pb.apply(p, b);
    BOOST_CHECK_EQUAL(result, return_type(0));
}

template <typename Point, typename Box>
inline void test_axis_3d()
{
    Box b;
    assign_values(b, 0, 0, 0, 1, 1, 1);
    Point p;
    bg::assign_values(p, 2, 0, 0);

    typedef bg::strategy::distance::pythagoras_point_box<> pythagoras_pb_type;
    typedef typename bg::strategy::distance::services::return_type
        <
            pythagoras_pb_type, Point, Box
        >::type return_type;

    pythagoras_pb_type pythagoras_pb;

    return_type result = pythagoras_pb.apply(p, b);
    BOOST_CHECK_EQUAL(result, return_type(1));

    bg::assign_values(p, 0, 2, 0);
    result = pythagoras_pb.apply(p, b);
    BOOST_CHECK_EQUAL(result, return_type(1));

    bg::assign_values(p, 0, 0, 2);
    result = pythagoras_pb.apply(p, b);
    BOOST_CHECK_CLOSE(result, return_type(1), 0.001);
}

template <typename Point, typename Box>
inline void test_arbitrary_3d()
{
    Box b;
    assign_values(b, 0, 0, 0, 1, 2, 3);
    Point p;
    bg::assign_values(p, 9, 8, 7);

    {
        typedef bg::strategy::distance::pythagoras_point_box<> strategy_type;
        typedef typename bg::strategy::distance::services::return_type
            <
                strategy_type, Point, Box
            >::type return_type;

        strategy_type strategy;
        return_type result = strategy.apply(p, b);
        BOOST_CHECK_CLOSE(result, return_type(10.77032961427), 0.001);
    }

    {
        // Check comparable distance
        typedef bg::strategy::distance::comparable::pythagoras_point_box<>
            strategy_type;

        typedef typename bg::strategy::distance::services::return_type
            <
                strategy_type, Point, Box
            >::type return_type;

        strategy_type strategy;
        return_type result = strategy.apply(p, b);
        BOOST_CHECK_EQUAL(result, return_type(116));
    }
}

template <typename Point, typename Box, typename CalculationType>
inline void test_services()
{
    namespace bgsd = bg::strategy::distance;
    namespace services = bg::strategy::distance::services;

    {
        // Compile-check if there is a strategy for this type
        typedef typename services::default_strategy
            <
                bg::point_tag, bg::box_tag, Point, Box
            >::type pythagoras_pb_strategy_type;

        // reverse geometry tags
        typedef typename services::default_strategy
            <
                bg::box_tag, bg::point_tag, Box, Point
            >::type reversed_pythagoras_pb_strategy_type;

        boost::ignore_unused
            <
                pythagoras_pb_strategy_type,
                reversed_pythagoras_pb_strategy_type
            >();
    }

    Point p;
    bg::assign_values(p, 1, 2, 3);

    Box b;
    assign_values(b, 4, 5, 6, 14, 15, 16);

    double const sqr_expected = 3*3 + 3*3 + 3*3; // 27
    double const expected = sqrt(sqr_expected); // sqrt(27)=5.1961524227

    // 1: normal, calculate distance:

    typedef bgsd::pythagoras_point_box<CalculationType> strategy_type;

    BOOST_CONCEPT_ASSERT
        ( (bg::concepts::PointDistanceStrategy<strategy_type, Point, Box>) );

    typedef typename bgsd::services::return_type
        <
            strategy_type, Point, Box
        >::type return_type;

    strategy_type strategy;
    return_type result = strategy.apply(p, b);
    BOOST_CHECK_CLOSE(result, return_type(expected), 0.001);

    // 2: the strategy should return the same result if we reverse parameters
    //    result = strategy.apply(p2, p1);
    //    BOOST_CHECK_CLOSE(result, return_type(expected), 0.001);


    // 3: "comparable" to construct a "comparable strategy" for Point/Box
    //    a "comparable strategy" is a strategy which does not calculate the exact distance, but
    //    which returns results which can be mutually compared (e.g. avoid sqrt)

    // 3a: "comparable_type"
    typedef typename services::comparable_type
        <
            strategy_type
        >::type comparable_type;

    // 3b: "get_comparable"
    comparable_type comparable = bgsd::services::get_comparable
        <
            strategy_type
        >::apply(strategy);

    typedef typename bgsd::services::return_type
        <
            comparable_type, Point, Box
        >::type comparable_return_type;

    comparable_return_type c_result = comparable.apply(p, b);
    BOOST_CHECK_CLOSE(c_result, return_type(sqr_expected), 0.001);

    // 4: the comparable_type should have a distance_strategy_constructor as well,
    //    knowing how to compare something with a fixed distance
    comparable_return_type c_dist5 = services::result_from_distance
        <
            comparable_type, Point, Box
        >::apply(comparable, 5.0);

    comparable_return_type c_dist6 = services::result_from_distance
        <
            comparable_type, Point, Box
        >::apply(comparable, 6.0);

    // If this is the case:
    BOOST_CHECK(c_dist5 < c_result && c_result < c_dist6);

    // This should also be the case
    return_type dist5 = services::result_from_distance
        <
            strategy_type, Point, Box
        >::apply(strategy, 5.0);
    return_type dist6 = services::result_from_distance
        <
            strategy_type, Point, Box
        >::apply(strategy, 6.0);
    BOOST_CHECK(dist5 < result && result < dist6);
}

template
<
    typename CoordinateType,
    typename CalculationType,
    typename AssignType
>
inline void test_big_2d_with(AssignType const& x1, AssignType const& y1,
                             AssignType const& x2, AssignType const& y2,
                             AssignType const& zero)
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;
    typedef bg::model::box<point_type> box_type;
    typedef bg::strategy::distance::pythagoras_point_box
        <
            CalculationType
        > pythagoras_pb_type;

    pythagoras_pb_type pythagoras_pb;
    typedef typename bg::strategy::distance::services::return_type
        <
            pythagoras_pb_type, point_type, box_type
        >::type return_type;


    point_type p;
    box_type b;
    bg::assign_values(b, zero, zero, x1, y1);
    bg::assign_values(p, x2, y2);
    return_type d = pythagoras_pb.apply(p, b);

    BOOST_CHECK_CLOSE(d, return_type(1076554.5485833955678294387789057), 0.001);
}

template <typename CoordinateType, typename CalculationType>
inline void test_big_2d()
{
    test_big_2d_with<CoordinateType, CalculationType>
        (123456.78900001, 234567.89100001,
         987654.32100001, 876543.21900001,
         0.0);
}

template <typename CoordinateType, typename CalculationType>
inline void test_big_2d_string()
{
    test_big_2d_with<CoordinateType, CalculationType>
        ("123456.78900001", "234567.89100001",
         "987654.32100001", "876543.21900001",
         "0.0000000000000");
}

template <typename CoordinateType>
inline void test_integer(bool check_types)
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;
    typedef bg::model::box<point_type> box_type;

    point_type p;
    box_type b;
    bg::assign_values(b, 0, 0, 12345678, 23456789);
    bg::assign_values(p, 98765432, 87654321);

    typedef bg::strategy::distance::pythagoras_point_box<> pythagoras_type;
    typedef typename bg::strategy::distance::services::comparable_type
        <
            pythagoras_type
        >::type comparable_type;

    typedef typename bg::strategy::distance::services::return_type
        <
            pythagoras_type, point_type, box_type
        >::type distance_type;
    typedef typename bg::strategy::distance::services::return_type
        <
            comparable_type, point_type, box_type
        >::type cdistance_type;
    
    pythagoras_type pythagoras;
    distance_type distance = pythagoras.apply(p, b);
    BOOST_CHECK_CLOSE(distance, 107655455.02347542, 0.001);

    comparable_type comparable;
    cdistance_type cdistance = comparable.apply(p, b);
    BOOST_CHECK_EQUAL(cdistance, 11589696996311540.0);

    distance_type distance2 = sqrt(distance_type(cdistance));
    BOOST_CHECK_CLOSE(distance, distance2, 0.001);

    if (check_types)
    {
        BOOST_CHECK((boost::is_same<distance_type, double>::type::value));
        BOOST_CHECK((boost::is_same<cdistance_type, boost::long_long_type>::type::value));
    }
}

template <typename P1, typename P2>
void test_all_3d()
{
    test_null_distance_3d<P1, bg::model::box<P2> >();
    test_axis_3d<P1, bg::model::box<P2> >();
    test_arbitrary_3d<P1, bg::model::box<P2> >();

    test_null_distance_3d<P2, bg::model::box<P1> >();
    test_axis_3d<P2, bg::model::box<P1> >();
    test_arbitrary_3d<P2, bg::model::box<P1> >();
}

template <typename P>
void test_all_3d()
{
    test_all_3d<P, int[3]>();
    test_all_3d<P, float[3]>();
    test_all_3d<P, double[3]>();
    test_all_3d<P, test::test_point>();
    test_all_3d<P, bg::model::point<int, 3, bg::cs::cartesian> >();
    test_all_3d<P, bg::model::point<float, 3, bg::cs::cartesian> >();
    test_all_3d<P, bg::model::point<double, 3, bg::cs::cartesian> >();
}

template <typename P, typename Strategy>
void time_compare_s(int const n)
{
    typedef bg::model::box<P> box_type;

    boost::timer t;
    P p;
    box_type b;
    bg::assign_values(b, 0, 0, 1, 1);
    bg::assign_values(p, 2, 2);
    Strategy strategy;
    typename bg::strategy::distance::services::return_type
        <
            Strategy, P, box_type
        >::type s = 0;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            bg::set<0>(p, bg::get<0>(p) + 0.001);
            s += strategy.apply(p, b);
        }
    }
    std::cout << "s: " << s << " t: " << t.elapsed() << std::endl;
}

template <typename P>
inline void time_compare(int const n)
{
    time_compare_s<P, bg::strategy::distance::pythagoras_point_box<> >(n);
    time_compare_s
        <
            P, bg::strategy::distance::comparable::pythagoras_point_box<>
        >(n);
}




BOOST_AUTO_TEST_CASE( test_integer_all )
{
    test_integer<int>(true);
    test_integer<boost::long_long_type>(true);
    test_integer<double>(false);
}


BOOST_AUTO_TEST_CASE( test_3d_all )
{
    test_all_3d<int[3]>();
    test_all_3d<float[3]>();
    test_all_3d<double[3]>();

    test_all_3d<test::test_point>();

    test_all_3d<bg::model::point<int, 3, bg::cs::cartesian> >();
    test_all_3d<bg::model::point<float, 3, bg::cs::cartesian> >();
    test_all_3d<bg::model::point<double, 3, bg::cs::cartesian> >();
}


BOOST_AUTO_TEST_CASE( test_big_2d_all )
{
    test_big_2d<float, float>();
    test_big_2d<double, double>();
    test_big_2d<long double, long double>();
    test_big_2d<float, long double>();
}


BOOST_AUTO_TEST_CASE( test_services_all )
{
    test_services
        <
            bg::model::point<float, 3, bg::cs::cartesian>,
            bg::model::box<double[3]>,
            long double
        >();
    test_services<double[3], bg::model::box<test::test_point>, float>();

    // reverse the point and box types
    test_services
        <
            double[3],
            bg::model::box<bg::model::point<float, 3, bg::cs::cartesian> >,
            long double
        >();
    test_services<test::test_point, bg::model::box<double[3]>, float>();
}


BOOST_AUTO_TEST_CASE( test_time_compare )
{
    // TODO move this to another non-unit test
    //    time_compare<bg::model::point<double, 2, bg::cs::cartesian> >(10000);
}


#if defined(HAVE_TTMATH)
BOOST_AUTO_TEST_CASE( test_ttmath_all )
{
    typedef ttmath::Big<1,4> tt;
    typedef bg::model::point<tt, 3, bg::cs::cartesian> tt_point;

    //test_all_3d<tt[3]>();
    test_all_3d<tt_point>();
    test_all_3d<tt_point, tt_point>();
    test_big_2d<tt, tt>();
    test_big_2d_string<tt, tt>();
}
#endif
