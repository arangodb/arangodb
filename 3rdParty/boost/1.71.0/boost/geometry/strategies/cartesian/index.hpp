// Boost.Geometry Index
//
// R-tree strategies
//
// Copyright (c) 2019, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_CARTESIAN_INDEX_HPP
#define BOOST_GEOMETRY_STRATEGIES_CARTESIAN_INDEX_HPP


#include <boost/geometry/strategies/cartesian/box_in_box.hpp>
#include <boost/geometry/strategies/cartesian/disjoint_box_box.hpp>
//#include <boost/geometry/strategies/cartesian/disjoint_segment_box.hpp>
#include <boost/geometry/strategies/cartesian/distance_projected_point.hpp>
#include <boost/geometry/strategies/cartesian/distance_pythagoras.hpp>
#include <boost/geometry/strategies/cartesian/distance_pythagoras_point_box.hpp>
#include <boost/geometry/strategies/cartesian/distance_segment_box.hpp>
#include <boost/geometry/strategies/cartesian/envelope_box.hpp>
#include <boost/geometry/strategies/cartesian/envelope_point.hpp>
#include <boost/geometry/strategies/cartesian/envelope_segment.hpp>
#include <boost/geometry/strategies/cartesian/expand_box.hpp>
#include <boost/geometry/strategies/cartesian/expand_point.hpp>
#include <boost/geometry/strategies/cartesian/expand_segment.hpp>
#include <boost/geometry/strategies/cartesian/intersection.hpp>
#include <boost/geometry/strategies/cartesian/point_in_box.hpp>
#include <boost/geometry/strategies/cartesian/point_in_point.hpp>
#include <boost/geometry/strategies/cartesian/point_in_poly_winding.hpp>

#include <boost/geometry/strategies/index.hpp>


namespace boost { namespace geometry { namespace strategy { namespace index
{

template
<
    typename CalculationType = void
>
struct cartesian
{
    typedef cartesian_tag cs_tag;

    typedef geometry::strategy::envelope::cartesian_point envelope_point_strategy_type;
    typedef geometry::strategy::envelope::cartesian_box envelope_box_strategy_type;
    typedef geometry::strategy::envelope::cartesian_segment
        <
            CalculationType
        > envelope_segment_strategy_type;

    static inline envelope_segment_strategy_type get_envelope_segment_strategy()
    {
        return envelope_segment_strategy_type();
    }

    typedef geometry::strategy::expand::cartesian_point expand_point_strategy_type;
    typedef geometry::strategy::expand::cartesian_box expand_box_strategy_type;
    typedef geometry::strategy::expand::cartesian_segment expand_segment_strategy_type;

    static inline expand_segment_strategy_type get_expand_segment_strategy()
    {
        return expand_segment_strategy_type();
    }

    typedef geometry::strategy::covered_by::cartesian_point_box covered_by_point_box_strategy_type;
    typedef geometry::strategy::covered_by::cartesian_box_box covered_by_box_box_strategy_type;
    typedef geometry::strategy::within::cartesian_point_point within_point_point_strategy_type;
    /*
    typedef geometry::strategy::within::cartesian_point_box within_point_box_strategy_type;
    typedef geometry::strategy::within::cartesian_box_box within_box_box_strategy_type;
    typedef geometry::strategy::within::cartesian_winding
        <
            void, void, CalculationType
        > within_point_segment_strategy_type;

    static inline within_point_segment_strategy_type get_within_point_segment_strategy()
    {
        return within_point_segment_strategy_type();
    }
    */
    
    // used in equals(Seg, Seg) but only to get_point_in_point_strategy()
    typedef geometry::strategy::intersection::cartesian_segments
        <
            CalculationType
        > relate_segment_segment_strategy_type;

    static inline relate_segment_segment_strategy_type get_relate_segment_segment_strategy()
    {
        return relate_segment_segment_strategy_type();
    }
    
    // used in intersection_content
    typedef geometry::strategy::disjoint::cartesian_box_box disjoint_box_box_strategy_type;
    
    typedef geometry::strategy::distance::comparable::pythagoras
        <
            CalculationType
        > comparable_distance_point_point_strategy_type;

    static inline comparable_distance_point_point_strategy_type get_comparable_distance_point_point_strategy()
    {
        return comparable_distance_point_point_strategy_type();
    }

    typedef geometry::strategy::distance::comparable::pythagoras_point_box
        <
            CalculationType
        > comparable_distance_point_box_strategy_type;

    static inline comparable_distance_point_box_strategy_type get_comparable_distance_point_box_strategy()
    {
        return comparable_distance_point_box_strategy_type();
    }

    // TODO: comparable version should be possible
    typedef geometry::strategy::distance::projected_point
        <
            CalculationType,
            geometry::strategy::distance::pythagoras<CalculationType>
        > comparable_distance_point_segment_strategy_type;

    static inline comparable_distance_point_segment_strategy_type get_comparable_distance_point_segment_strategy()
    {
        return comparable_distance_point_segment_strategy_type();
    }

    typedef geometry::strategy::distance::cartesian_segment_box
        <
            CalculationType,
            geometry::strategy::distance::pythagoras<CalculationType>
        > comparable_distance_segment_box_strategy_type;

    static inline comparable_distance_segment_box_strategy_type get_comparable_distance_segment_box_strategy()
    {
        return comparable_distance_segment_box_strategy_type();
    }
};


namespace services
{

template <typename Geometry>
struct default_strategy<Geometry, cartesian_tag>
{
    typedef cartesian<> type;
};


// within and relate (MPt, Mls/MPoly)
template <typename Point1, typename Point2, typename CalculationType>
struct from_strategy<within::cartesian_winding<Point1, Point2, CalculationType> >
{
    typedef strategy::index::cartesian<CalculationType> type;

    static inline type get(within::cartesian_winding<Point1, Point2, CalculationType> const&)
    {
        return type();
    }
};

// distance (MPt, MPt)
template <typename CalculationType>
struct from_strategy<distance::comparable::pythagoras<CalculationType> >
{
    typedef strategy::index::cartesian<CalculationType> type;

    static inline type get(distance::comparable::pythagoras<CalculationType> const&)
    {
        return type();
    }
};

// distance (MPt, Linear/Areal)
template <typename CalculationType, typename PPStrategy>
struct from_strategy<distance::projected_point<CalculationType, PPStrategy> >
{
    typedef strategy::index::cartesian<CalculationType> type;

    static inline type get(distance::projected_point<CalculationType, PPStrategy> const&)
    {
        return type();
    }
};


} // namespace services


}}}} // namespace boost::geometry::strategy::index

#endif // BOOST_GEOMETRY_STRATEGIES_CARTESIAN_INDEX_HPP
