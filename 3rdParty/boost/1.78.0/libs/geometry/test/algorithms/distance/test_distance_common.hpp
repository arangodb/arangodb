// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014-2021, Oracle and/or its affiliates.
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_DISTANCE_COMMON_HPP
#define BOOST_GEOMETRY_TEST_DISTANCE_COMMON_HPP

#include <iostream>
#include <string>

#include <boost/math/special_functions/fpclassify.hpp>

#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/comparable_distance.hpp>
#include <boost/geometry/algorithms/num_interior_rings.hpp>

#include <boost/geometry/geometries/geometries.hpp>

#include <boost/geometry/io/wkt/write.hpp>
#include <boost/geometry/io/dsv/write.hpp>

#include <boost/geometry/strategies/strategies.hpp>

#include <from_wkt.hpp>
#include <string_from_type.hpp>


#ifndef BOOST_GEOMETRY_TEST_DISTANCE_HPP

namespace bg = ::boost::geometry;

// function copied from BG's test_distance.hpp

template <typename Geometry1, typename Geometry2>
void test_empty_input(Geometry1 const& geometry1, Geometry2 const& geometry2)
{
    try
    {
        bg::distance(geometry1, geometry2);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false, "A empty_input_exception should have been thrown" );
}
#endif // BOOST_GEOMETRY_TEST_DISTANCE_HPP

//========================================================================



#ifdef BOOST_GEOMETRY_TEST_DEBUG
// pretty print geometry -- START
template <typename Geometry, typename GeometryTag>
struct pretty_print_geometry_dispatch
{
    template <typename Stream>
    static inline Stream& apply(Geometry const& geometry, Stream& os)
    {
        os << bg::wkt(geometry);
        return os;
    }
};

template <typename Geometry>
struct pretty_print_geometry_dispatch<Geometry, bg::segment_tag>
{
    template <typename Stream>
    static inline Stream& apply(Geometry const& geometry, Stream& os)
    {
        os << "SEGMENT" << bg::dsv(geometry);
        return os;
    }
};

template <typename Geometry>
struct pretty_print_geometry_dispatch<Geometry, bg::box_tag>
{
    template <typename Stream>
    static inline Stream& apply(Geometry const& geometry, Stream& os)
    {
        os << "BOX" << bg::dsv(geometry);
        return os;
    }
};


template <typename Geometry>
struct pretty_print_geometry
{
    template <typename Stream>
    static inline Stream& apply(Geometry const& geometry, Stream& os)
    {
        return pretty_print_geometry_dispatch
            <
                Geometry, typename bg::tag<Geometry>::type
            >::apply(geometry, os);
    }
};
// pretty print geometry -- END
#endif // BOOST_GEOMETRY_TEST_DEBUG


//========================================================================


template <typename T>
struct check_equal
{
    static inline void apply(T const& detected, T const& expected,
                             bool is_finite)
    {
        if (is_finite)
        {
            BOOST_CHECK(detected == expected);
        }
        else
        {
            BOOST_CHECK(! boost::math::isfinite(detected));
        }
    }
};

template <>
struct check_equal<double>
{
    static inline void apply(double detected, double expected,
                             bool is_finite)
    {
        if (is_finite)
        {
            BOOST_CHECK_CLOSE(detected, expected, 0.0001);
        }
        else
        {
            BOOST_CHECK(! boost::math::isfinite(detected));
        }
    }
};


//========================================================================

template
<
    typename Geometry1, typename Geometry2,
    int id1 = bg::geometry_id<Geometry1>::value,
    int id2 = bg::geometry_id<Geometry2>::value
>
struct test_distance_of_geometries
    : public test_distance_of_geometries<Geometry1, Geometry2, 0, 0>
{};

#ifdef BOOST_GEOMETRY_TEST_DEBUG
#define ENABLE_IF_DEBUG(ID) ID
#else
#define ENABLE_IF_DEBUG(ID)
#endif

template <typename Geometry1, typename Geometry2>
class test_distance_of_geometries<Geometry1, Geometry2, 0, 0>
{
private:
    template
    <
        typename G1,
        typename G2,
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void base_test(std::string const& ENABLE_IF_DEBUG(header),
                   G1 const& g1, G2 const& g2,
                   DistanceType const& expected_distance,
                   ComparableDistanceType const& expected_comparable_distance,
                   Strategy const& strategy,
                   bool is_finite)
    {
        typedef typename bg::default_distance_result
            <
                G1, G2
            >::type default_distance_result;

        typedef typename bg::strategy::distance::services::return_type
            <
                Strategy, G1, G2
            >::type distance_result_from_strategy;

        static const bool same_regular = std::is_same
            <
                default_distance_result,
                distance_result_from_strategy
            >::value;

        BOOST_CHECK( same_regular );
    

        typedef typename bg::default_comparable_distance_result
            <
                G1, G2
            >::type default_comparable_distance_result;

        typedef typename bg::strategy::distance::services::return_type
            <
                typename bg::strategy::distance::services::comparable_type
                    <
                        Strategy
                    >::type,
                G1,
                G2
            >::type comparable_distance_result_from_strategy;

        static const bool same_comparable = std::is_same
            <
                default_comparable_distance_result,
                comparable_distance_result_from_strategy
            >::value;
        
        BOOST_CHECK( same_comparable );


        // check distance with default strategy
        default_distance_result dist_def = bg::distance(g1, g2);

        check_equal
            <
                default_distance_result
            >::apply(dist_def, expected_distance, is_finite);


        // check distance with passed strategy
        distance_result_from_strategy dist = bg::distance(g1, g2, strategy);

        check_equal
            <
                default_distance_result
            >::apply(dist, expected_distance, is_finite);


        // check comparable distance with default strategy
        default_comparable_distance_result cdist_def =
            bg::comparable_distance(g1, g2);

        check_equal
            <
                default_comparable_distance_result
            >::apply(cdist_def, expected_comparable_distance, is_finite);


        // check comparable distance with passed strategy
        comparable_distance_result_from_strategy cdist =
            bg::comparable_distance(g1, g2, strategy);

        check_equal
            <
                default_comparable_distance_result
            >::apply(cdist, expected_comparable_distance, is_finite);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << string_from_type<typename bg::coordinate_type<Geometry1>::type>::name()
                  << string_from_type<typename bg::coordinate_type<Geometry2>::type>::name()
                  << " -> "
                  << string_from_type<default_distance_result>::name()
                  << string_from_type<default_comparable_distance_result>::name()
                  << std::endl;

        std::cout << "distance" << header
                  << " (def. strategy) = " << dist_def << " ; "
                  << "distance" << header
                  <<" (passed strategy) = " << dist << " ; "
                  << "comp. distance" << header <<" (def. strategy) = "
                  << cdist_def << " ; "
                  << "comp. distance" << header <<" (passed strategy) = "
                  << cdist << std::endl;
#endif
    }

public:
    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(std::string const& wkt1,
               std::string const& wkt2,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool is_finite = true)
    {
        Geometry1 geometry1 = from_wkt<Geometry1>(wkt1);
        Geometry2 geometry2 = from_wkt<Geometry2>(wkt2);

        apply(geometry1, geometry2,
              expected_distance, expected_comparable_distance,
              strategy, is_finite);
    }


    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(Geometry1 const& geometry1,
               Geometry2 const& geometry2,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool is_finite = true)
    {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
        typedef pretty_print_geometry<Geometry1> PPG1;
        typedef pretty_print_geometry<Geometry2> PPG2;
        PPG1::apply(geometry1, std::cout);
        std::cout << " - ";
        PPG2::apply(geometry2, std::cout);
        std::cout << std::endl;
#endif

        base_test("", geometry1, geometry2,
                  expected_distance, expected_comparable_distance,
                  strategy, is_finite);

        base_test("[reversed args]", geometry2, geometry1,
                  expected_distance, expected_comparable_distance,
                  strategy, is_finite);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << std::endl;
#endif
    }
};


//========================================================================

template <typename Segment, typename Polygon>
struct test_distance_of_geometries
<
    Segment, Polygon,
    92 /* segment */, 3 /* polygon */
>
    : public test_distance_of_geometries<Segment, Polygon, 0, 0>
{
    typedef test_distance_of_geometries<Segment, Polygon, 0, 0> base;

    typedef typename bg::ring_type<Polygon>::type ring_type;

    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(std::string const& wkt_segment,
               std::string const& wkt_polygon,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool is_finite = true)
    {
        Segment segment = from_wkt<Segment>(wkt_segment);
        Polygon polygon = from_wkt<Polygon>(wkt_polygon);
        apply(segment,
              polygon,
              expected_distance,
              expected_comparable_distance,
              strategy,
              is_finite);
    }


    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(Segment const& segment,
               Polygon const& polygon,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool is_finite = true)
    {
        base::apply(segment, polygon, expected_distance,
                    expected_comparable_distance, strategy, is_finite);

        if ( bg::num_interior_rings(polygon) == 0 ) {
#ifdef BOOST_GEOMETRY_TEST_DEBUG
            std::cout << "... testing also exterior ring ..." << std::endl;
#endif
            test_distance_of_geometries
                <
                    Segment, ring_type
                >::apply(segment,
                         bg::exterior_ring(polygon),
                         expected_distance,
                         expected_comparable_distance,
                         strategy,
                         is_finite);
        }
    }
};

//========================================================================

template <typename Box, typename Segment>
struct test_distance_of_geometries
<
    Box, Segment,
    94 /* box */, 92 /* segment */
>
{
    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(std::string const& wkt_box,
               std::string const& wkt_segment,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool is_finite = true)
    {
        test_distance_of_geometries
            <
                Segment, Box, 92, 94
            >::apply(wkt_segment,
                     wkt_box,
                     expected_distance,
                     expected_comparable_distance,
                     strategy,
                     is_finite);
    }
};


template <typename Segment, typename Box>
struct test_distance_of_geometries
<
    Segment, Box,
    92 /* segment */, 94 /* box */
>
    : public test_distance_of_geometries<Segment, Box, 0, 0>
{
    typedef test_distance_of_geometries<Segment, Box, 0, 0> base;

    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(std::string const& wkt_segment,
               std::string const& wkt_box,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool is_finite = true)
    {
        Segment segment = from_wkt<Segment>(wkt_segment);
        Box box = from_wkt<Box>(wkt_box);
        apply(segment,
              box,
              expected_distance,
              expected_comparable_distance,
              strategy,
              is_finite);
    }


    template
    <
        typename DistanceType,
        typename ComparableDistanceType,
        typename Strategy
    >
    static inline
    void apply(Segment const& segment,
               Box const& box,
               DistanceType const& expected_distance,
               ComparableDistanceType const& expected_comparable_distance,
               Strategy const& strategy,
               bool is_finite = true)
    {
        typedef typename bg::strategy::distance::services::return_type
            <
                Strategy, Segment, Box
            >::type distance_result_type;

        typedef typename bg::strategy::distance::services::comparable_type
            <
                Strategy
            >::type comparable_strategy;

        typedef typename bg::strategy::distance::services::return_type
            <
                comparable_strategy, Segment, Box
            >::type comparable_distance_result_type;


        base::apply(segment, box, expected_distance,
                    expected_comparable_distance, strategy, is_finite);

        auto strategies = bg::strategies::distance::services::strategy_converter<Strategy>::get(strategy);
        auto cstrategies = bg::strategies::distance::detail::make_comparable(strategies);

        // TODO: these algorithms are used only here. Remove them?

        distance_result_type distance_generic =
            bg::detail::distance::segment_to_box_2D_generic
                <
                    Segment, Box, decltype(strategies), false
                >::apply(segment, box, strategies);

        comparable_distance_result_type comparable_distance_generic =
            bg::detail::distance::segment_to_box_2D_generic
                <
                    Segment, Box, decltype(cstrategies), false
                >::apply(segment, box, cstrategies);

        check_equal
            <
                distance_result_type
            >::apply(distance_generic, expected_distance, is_finite);

        check_equal
            <
                comparable_distance_result_type
            >::apply(comparable_distance_generic,
                     expected_comparable_distance,
                     is_finite);

        distance_generic =
            bg::detail::distance::segment_to_box_2D_generic
                <
                    Segment, Box, decltype(strategies), true
                >::apply(segment, box, strategies);

        comparable_distance_generic =
            bg::detail::distance::segment_to_box_2D_generic
                <
                    Segment, Box, decltype(cstrategies), true
                >::apply(segment, box, cstrategies);

        check_equal
            <
                distance_result_type
            >::apply(distance_generic, expected_distance, is_finite);

        check_equal
            <
                comparable_distance_result_type
            >::apply(comparable_distance_generic,
                     expected_comparable_distance,
                     is_finite);

#ifdef BOOST_GEOMETRY_TEST_DEBUG
        std::cout << "... testing with naive seg-box distance algorithm..."
                  << std::endl;
        std::cout << "distance (generic algorithm) = "
                  << distance_generic << " ; "
                  << "comp. distance (generic algorithm) = "            
                  << comparable_distance_generic
                  << std::endl;
        std::cout << std::endl << std::endl;
#endif
    }
};

//========================================================================


template <typename Geometry1, typename Geometry2, typename Strategy>
void test_empty_input(Geometry1 const& geometry1,
                      Geometry2 const& geometry2,
                      Strategy const& strategy)
{
    try
    {
        bg::distance(geometry1, geometry2, strategy);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false, "A empty_input_exception should have been thrown" );

    try
    {
        bg::distance(geometry2, geometry1, strategy);
    }
    catch(bg::empty_input_exception const& )
    {
        return;
    }
    BOOST_CHECK_MESSAGE(false, "A empty_input_exception should have been thrown" );
}

#endif // BOOST_GEOMETRY_TEST_DISTANCE_COMMON_HPP
