// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE test_douglas_peucker
#endif

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#ifndef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
#define BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
#endif
#endif

#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include <boost/test/included/unit_test.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/tuple/tuple.hpp>

#include <boost/geometry/core/point_type.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/strategies/distance.hpp>
#include <boost/geometry/strategies/strategies.hpp>

#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/adapted/boost_tuple.hpp>
#include <boost/geometry/geometries/register/multi_point.hpp>

#include <boost/geometry/algorithms/comparable_distance.hpp>
#include <boost/geometry/algorithms/equals.hpp>

#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/io/dsv/write.hpp>


namespace bg = ::boost::geometry;
namespace ba = ::boost::assign;
namespace services = bg::strategy::distance::services;

typedef boost::tuple<double, double> tuple_point_type;
typedef std::vector<tuple_point_type> tuple_multi_point_type;

BOOST_GEOMETRY_REGISTER_BOOST_TUPLE_CS(cs::cartesian)
BOOST_GEOMETRY_REGISTER_MULTI_POINT(tuple_multi_point_type)
BOOST_GEOMETRY_REGISTER_MULTI_POINT_TEMPLATED(std::vector)

typedef bg::strategy::distance::projected_point<> distance_strategy_type;
typedef bg::strategy::distance::projected_point
    <
        void, bg::strategy::distance::comparable::pythagoras<>
    > comparable_distance_strategy_type;


template <typename CoordinateType>
struct default_simplify_strategy
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;
    typedef typename bg::strategy::distance::services::default_strategy
        <
            bg::point_tag, bg::segment_tag, point_type
        >::type default_distance_strategy_type;

    typedef bg::strategy::simplify::douglas_peucker
        <
            point_type, default_distance_strategy_type
        > type;
};


template <typename CoordinateType>
struct simplify_regular_distance_strategy
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;
    typedef bg::strategy::simplify::douglas_peucker
        <
            point_type, distance_strategy_type
        > type;
};

template <typename CoordinateType>
struct simplify_comparable_distance_strategy
{
    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;
    typedef bg::strategy::simplify::douglas_peucker
        <
            point_type, comparable_distance_strategy_type
        > type;
};



template <typename Geometry>
inline Geometry from_wkt(std::string const& wkt)
{
    Geometry geometry;
    boost::geometry::read_wkt(wkt, geometry);
    return geometry;
}

template <typename Iterator>
inline std::ostream& print_point_range(std::ostream& os,
                                       Iterator first,
                                       Iterator beyond,
                                       std::string const& header)
{
    os << header << "(";
    for (Iterator it = first; it != beyond; ++it)
    {
        os << " " << bg::dsv(*it);
    }
    os << " )";
    return os;
}


struct equals
{
    template <typename Iterator1, typename Iterator2>
    static inline bool apply(Iterator1 begin1, Iterator1 end1,
                             Iterator2 begin2, Iterator2 end2)
    {
        std::size_t num_points1 = std::distance(begin1, end1);
        std::size_t num_points2 = std::distance(begin2, end2);

        if ( num_points1 != num_points2 )
        {
            return false;
        }

        Iterator1 it1 = begin1;
        Iterator2 it2 = begin2;
        for (; it1 != end1; ++it1, ++it2)
        {
            if ( !bg::equals(*it1, *it2) )
            {
                return false;
            }
        }
        return true;
    }

    template <typename Range1, typename Range2>
    static inline bool apply(Range1 const& range1, Range2 const& range2)
    {
        return apply(boost::begin(range1), boost::end(range1),
                     boost::begin(range2), boost::end(range2));
    }
};




template <typename Geometry>
struct test_one_case
{
    template <typename Strategy, typename Range>
    static inline void apply(std::string const& case_id,
                             std::string const& wkt,
                             double max_distance,
                             Strategy const& strategy,
                             Range const& expected_result)
    {
        typedef typename bg::point_type<Geometry>::type point_type;
        std::vector<point_type> result;

        Geometry geometry = from_wkt<Geometry>(wkt);

        std::string typeid_name
            = typeid(typename bg::coordinate_type<point_type>::type).name();

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << case_id << " - " << typeid_name
                  << std::endl;
        std::cout << wkt << std::endl;
#endif

        strategy.apply(geometry, std::back_inserter(result), max_distance);

        boost::ignore_unused(strategy);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        print_point_range(std::cout, boost::begin(result), boost::end(result),
                          "output: ");
        std::cout << std::endl << std::endl;
#endif
        std::stringstream stream_expected;
        print_point_range(stream_expected, boost::begin(expected_result),
                          boost::end(expected_result),
                          "");
        std::stringstream stream_detected;
        print_point_range(stream_detected, boost::begin(result),
                          boost::end(result),
                          "");

        BOOST_CHECK_MESSAGE(equals::apply(result, expected_result),
                            "case id: " << case_id << " - " << typeid_name
                            << ", geometry: " << wkt
                            << ", Expected: " << stream_expected.str()
                            << " - Detected: " << stream_detected.str());

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "---------------" << std::endl;
        std::cout << "---------------" << std::endl;
        std::cout << std::endl << std::endl;
#endif
    }
};


template <typename CoordinateType, typename Strategy>
inline void test_with_strategy(std::string label)
{
    std::cout.precision(20);
    Strategy strategy;

    typedef bg::model::point<CoordinateType, 2, bg::cs::cartesian> point_type;
    typedef bg::model::linestring<point_type> linestring_type;
    typedef bg::model::segment<point_type> segment_type;
    typedef test_one_case<linestring_type> tester;

    label = " (" + label + ")";

    {
        point_type const p1(-6,-13), p2(0,-15);
        segment_type const s(point_type(12,-3), point_type(-12,5));

        if (bg::comparable_distance(p1, s) >= bg::comparable_distance(p2, s))
        {
            tester::apply("l01c1" + label,
                          "LINESTRING(12 -3, 4 8,-6 -13,-9 4,0 -15,-12 5)",
                          10,
                          strategy,
                          ba::tuple_list_of(12,-3)(4,8)(-6,-13)(-12,5)
                          );
        }
        else
        {
            tester::apply("l01c2" + label,
                          "LINESTRING(12 -3, 4 8,-6 -13,-9 4,0 -15,-12 5)",
                          10,
                          strategy,
                          ba::tuple_list_of(12,-3)(4,8)(-6,-13)(-9,4)(0,-15)(-12,5)
                          );
        }
    }

    tester::apply("l02" + label,
                  "LINESTRING(-6 -13,-9 4,0 -15,-12 5)",
                  10,
                  strategy,
                  ba::tuple_list_of(-6,-13)(-12,5)
                  );

    tester::apply("l03" + label,
                  "LINESTRING(12 -3, 4 8,-6 -13,-9 4,0 -14,-12 5)",
                  10,
                  strategy,
                  ba::tuple_list_of(12,-3)(4,8)(-6,-13)(-12,5)
                  );

    tester::apply("l04" + label,
                  "LINESTRING(12 -3, 4 8,-6 -13,-9 4,0 -14,-12 5)",
                  14,
                  strategy,
                  ba::tuple_list_of(12,-3)(-6,-13)(-12,5)
                  );

    {
        segment_type const s(point_type(0,-1), point_type(5,-4));
        point_type const p1(5,-1), p2(0,-4);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        bool d_larger_first = (bg::distance(p1, s) > bg::distance(p2, s));
        bool d_larger_second = (bg::distance(p1, s) < bg::distance(p2, s));
        bool cd_larger_first
            = (bg::comparable_distance(p1, s) > bg::comparable_distance(p2, s));
        bool cd_larger_second
            = (bg::comparable_distance(p1, s) < bg::comparable_distance(p2, s));

        std::cout << "segment: " << bg::dsv(s) << std::endl;
        std::cout << "distance from " << bg::dsv(p1) << ": "
                  << bg::distance(p1, s) << std::endl;
        std::cout << "comp. distance from " << bg::dsv(p1) << ": "
                  << bg::comparable_distance(p1, s) << std::endl;
        std::cout << "distance from " << bg::dsv(p2) << ": "
                  << bg::distance(p2, s) << std::endl;
        std::cout << "comp. distance from " << bg::dsv(p2) << ": "
                  << bg::comparable_distance(p2, s) << std::endl;
        std::cout << "larger distance from "
                  << (d_larger_first ? "first" : (d_larger_second ? "second" : "equal"))
                  << std::endl;
        std::cout << "larger comp. distance from "
                  << (cd_larger_first ? "first" : (cd_larger_second ? "second" : "equal"))
                  << std::endl;
        std::cout << "difference of distances: "
                  << (bg::distance(p1, s) - bg::distance(p2, s))
                  << std::endl;
        std::cout << "difference of comp. distances: "
                  << (bg::comparable_distance(p1, s) - bg::comparable_distance(p2, s))
                  << std::endl;
#endif

        std::string wkt =
            "LINESTRING(0 0,5 0,0 -1,5 -1,0 -2,5 -2,0 -3,5 -3,0 -4,5 -4,0 0)";

        if (bg::comparable_distance(p1, s) >= bg::comparable_distance(p2, s))
        {
            tester::apply("l05c1" + label,
                          wkt,
                          1,
                          strategy,
                          ba::tuple_list_of(0,0)(5,0)(0,-1)(5,-1)(0,-2)(5,-2)(0,-3)(5,-4)(0,0)
                          );
            tester::apply("l05c1a" + label,
                          wkt,
                          2,
                          strategy,
                          ba::tuple_list_of(0,0)(5,0)(0,-1)(5,-1)(0,-2)(5,-4)(0,0)
                          );
        }
        else
        {
            tester::apply("l05c2" + label,
                          wkt,
                          1,
                          strategy,
                          ba::tuple_list_of(0,0)(5,0)(0,-1)(5,-1)(0,-2)(5,-2)(0,-4)(5,-4)(0,0)
                          );
            tester::apply("l05c2a" + label,
                          wkt,
                          2,
                          strategy,
                          ba::tuple_list_of(0,0)(5,0)(0,-1)(5,-1)(0,-4)(5,-4)(0,0)
                          );
        }
    }

#ifdef BOOST_GEOMETRY_TEST_DEBUG
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "*************************************************";
    std::cout << std::endl;
    std::cout << std::endl;
#endif
}


BOOST_AUTO_TEST_CASE( test_default_strategy )
{
    test_with_strategy<int, default_simplify_strategy<int>::type>("i");
    test_with_strategy<float, default_simplify_strategy<float>::type>("f");
    test_with_strategy<double, default_simplify_strategy<double>::type>("d");
    test_with_strategy
        <
            long double,
            default_simplify_strategy<long double>::type
        >("ld");
}

BOOST_AUTO_TEST_CASE( test_with_regular_distance_strategy )
{
    test_with_strategy
        <
            int,
            simplify_regular_distance_strategy<int>::type
        >("i");

    test_with_strategy
        <
            float,
            simplify_regular_distance_strategy<float>::type
        >("f");

    test_with_strategy
        <
            double,
            simplify_regular_distance_strategy<double>::type
        >("d");
    test_with_strategy
        <
            long double,
            simplify_regular_distance_strategy<long double>::type
        >("ld");
}

BOOST_AUTO_TEST_CASE( test_with_comparable_distance_strategy )
{
    test_with_strategy
        <
            int,
            simplify_comparable_distance_strategy<int>::type
        >("i");
    test_with_strategy
        <
            float,
            simplify_comparable_distance_strategy<float>::type
        >("f");
    test_with_strategy
        <
            double,
            simplify_comparable_distance_strategy<double>::type
        >("d");
    test_with_strategy
        <
            long double,
            simplify_comparable_distance_strategy<long double>::type
        >("ld");
}
