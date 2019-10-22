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

#ifndef BOOST_GEOMETRY_STRATEGIES_SPHERICAL_INDEX_HPP
#define BOOST_GEOMETRY_STRATEGIES_SPHERICAL_INDEX_HPP

#include <boost/geometry/strategies/spherical/distance_haversine.hpp>
#include <boost/geometry/strategies/spherical/distance_cross_track.hpp>
#include <boost/geometry/strategies/spherical/distance_cross_track_point_box.hpp>
#include <boost/geometry/strategies/spherical/distance_segment_box.hpp>
#include <boost/geometry/strategies/spherical/envelope_box.hpp>
#include <boost/geometry/strategies/spherical/envelope_point.hpp>
#include <boost/geometry/strategies/spherical/envelope_segment.hpp>
#include <boost/geometry/strategies/spherical/expand_box.hpp>
#include <boost/geometry/strategies/spherical/expand_point.hpp>
#include <boost/geometry/strategies/spherical/expand_segment.hpp>
#include <boost/geometry/strategies/spherical/intersection.hpp>
#include <boost/geometry/strategies/spherical/point_in_poly_winding.hpp>

#include <boost/geometry/strategies/index.hpp>


namespace boost { namespace geometry { namespace strategy { namespace index
{

template
<
    typename CalculationType = void
>
struct spherical
{
    typedef spherical_tag cs_tag;

    typedef geometry::strategy::envelope::spherical_point envelope_point_strategy_type;
    typedef geometry::strategy::envelope::spherical_box envelope_box_strategy_type;
    typedef geometry::strategy::envelope::spherical_segment
        <
            CalculationType
        > envelope_segment_strategy_type;

    static inline envelope_segment_strategy_type get_envelope_segment_strategy()
    {
        return envelope_segment_strategy_type();
    }

    typedef geometry::strategy::expand::spherical_point expand_point_strategy_type;
    typedef geometry::strategy::expand::spherical_box expand_box_strategy_type;
    typedef geometry::strategy::expand::spherical_segment
        <
            CalculationType
        > expand_segment_strategy_type;

    static inline expand_segment_strategy_type get_expand_segment_strategy()
    {
        return expand_segment_strategy_type();
    }

	typedef geometry::strategy::covered_by::spherical_point_box covered_by_point_box_strategy_type;
	typedef geometry::strategy::covered_by::spherical_box_box covered_by_box_box_strategy_type;
	typedef geometry::strategy::within::spherical_point_point within_point_point_strategy_type;
	//typedef geometry::strategy::within::spherical_point_box within_point_box_strategy_type;
	//typedef geometry::strategy::within::spherical_box_box within_box_box_strategy_type;

    // used in equals(Seg, Seg) but only to get_point_in_point_strategy()
    typedef geometry::strategy::intersection::spherical_segments
        <
            CalculationType
        > relate_segment_segment_strategy_type;

    static inline relate_segment_segment_strategy_type get_relate_segment_segment_strategy()
    {
        return relate_segment_segment_strategy_type();
    }
    
    // used in intersection_content
    typedef geometry::strategy::disjoint::spherical_box_box disjoint_box_box_strategy_type;

    typedef geometry::strategy::distance::comparable::haversine
        <
            double,
            CalculationType
        > comparable_distance_point_point_strategy_type;

    static inline comparable_distance_point_point_strategy_type get_comparable_distance_point_point_strategy()
    {
        return comparable_distance_point_point_strategy_type();
    }

    // TODO: Comparable version should be possible
    typedef geometry::strategy::distance::cross_track_point_box
        <
            CalculationType,
            geometry::strategy::distance::haversine<double, CalculationType>
        > comparable_distance_point_box_strategy_type;

    static inline comparable_distance_point_box_strategy_type get_comparable_distance_point_box_strategy()
    {
        return comparable_distance_point_box_strategy_type();
    }

    // TODO: Radius is not needed in comparable strategy
    typedef geometry::strategy::distance::comparable::cross_track
        <
            CalculationType,
            geometry::strategy::distance::comparable::haversine<double, CalculationType>
        > comparable_distance_point_segment_strategy_type;

    static inline comparable_distance_point_segment_strategy_type get_comparable_distance_point_segment_strategy()
    {
        return comparable_distance_point_segment_strategy_type();
    }

    // comparable?
    typedef geometry::strategy::distance::spherical_segment_box
        <
            CalculationType,
            geometry::strategy::distance::haversine<double, CalculationType>
        > comparable_distance_segment_box_strategy_type;

    static inline comparable_distance_segment_box_strategy_type get_comparable_distance_segment_box_strategy()
    {
        return comparable_distance_segment_box_strategy_type();
    }
};


namespace services
{

template <typename Geometry>
struct default_strategy<Geometry, spherical_tag>
{
    typedef spherical<> type;
};

template <typename Geometry>
struct default_strategy<Geometry, spherical_polar_tag>
{
    typedef spherical<> type;
};

template <typename Geometry>
struct default_strategy<Geometry, spherical_equatorial_tag>
{
    typedef spherical<> type;
};


template <typename Point1, typename Point2, typename CalculationType>
struct from_strategy<within::spherical_winding<Point1, Point2, CalculationType> >
{
    typedef strategy::index::spherical<CalculationType> type;

    static inline type get(within::spherical_winding<Point1, Point2, CalculationType> const&)
    {
        return type();
    }
};

// distance (MPt, MPt)
template <typename RadiusType, typename CalculationType>
struct from_strategy<distance::comparable::haversine<RadiusType, CalculationType> >
{
    typedef strategy::index::spherical<CalculationType> type;

    static inline type get(distance::comparable::haversine<RadiusType, CalculationType> const&)
    {
        return type();
    }
};

// distance (MPt, Linear/Areal)
template <typename CalculationType, typename PPStrategy>
struct from_strategy<distance::comparable::cross_track<CalculationType, PPStrategy> >
{
    typedef strategy::index::spherical<CalculationType> type;

    static inline type get(distance::comparable::cross_track<CalculationType, PPStrategy> const&)
    {
        return type();
    }
};


} // namespace services


}}}} // namespace boost::geometry::strategy::index

#endif // BOOST_GEOMETRY_STRATEGIES_SPHERICAL_INDEX_HPP
