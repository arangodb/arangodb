// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2018 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_SPHERICAL_DISTANCE_SEGMENT_BOX_HPP
#define BOOST_GEOMETRY_STRATEGIES_SPHERICAL_DISTANCE_SEGMENT_BOX_HPP

#include <boost/geometry/algorithms/detail/distance/segment_to_box.hpp>

namespace boost { namespace geometry
{


namespace strategy { namespace distance
{

struct generic_segment_box
{
    template
    <
            typename LessEqual,
            typename ReturnType,
            typename SegmentPoint,
            typename BoxPoint,
            typename SegmentBoxStrategy,
            typename AzimuthStrategy,
            typename EnvelopeSegmentStrategy
    >
    static inline ReturnType segment_below_of_box(
            SegmentPoint const& p0,
            SegmentPoint const& p1,
            BoxPoint const&,
            BoxPoint const& top_right,
            BoxPoint const& bottom_left,
            BoxPoint const& bottom_right,
            SegmentBoxStrategy const& sb_strategy,
            AzimuthStrategy & az_strategy,
            EnvelopeSegmentStrategy & es_strategy)
    {
        ReturnType result;
        typename LessEqual::other less_equal;
        typedef geometry::model::segment<SegmentPoint> Segment;
        typedef typename cs_tag<Segment>::type segment_cs_type;
        typedef geometry::detail::disjoint::
                disjoint_segment_box_sphere_or_spheroid<segment_cs_type>
                disjoint_sb;
        typedef typename disjoint_sb::disjoint_info disjoint_info_type;

        Segment seg(p0, p1);

        geometry::model::box<BoxPoint> input_box;
        geometry::set_from_radian<geometry::min_corner, 0>
                (input_box, geometry::get_as_radian<0>(bottom_left));
        geometry::set_from_radian<geometry::min_corner, 1>
                (input_box, geometry::get_as_radian<1>(bottom_left));
        geometry::set_from_radian<geometry::max_corner, 0>
                (input_box, geometry::get_as_radian<0>(top_right));
        geometry::set_from_radian<geometry::max_corner, 1>
                (input_box, geometry::get_as_radian<1>(top_right));

        SegmentPoint p_max;

        disjoint_info_type disjoint_result = disjoint_sb::
                apply(seg, input_box, az_strategy, p_max);

        if (disjoint_result == disjoint_info_type::intersect) //intersect
        {
            return 0;
        }
        // disjoint but vertex not computed
        if (disjoint_result == disjoint_info_type::disjoint_no_vertex)
        {
            typedef typename coordinate_type<SegmentPoint>::type CT;

            geometry::model::box<SegmentPoint> mbr;
            geometry::envelope(seg, mbr, es_strategy);

            CT lon1 = geometry::get_as_radian<0>(p0);
            CT lat1 = geometry::get_as_radian<1>(p0);
            CT lon2 = geometry::get_as_radian<0>(p1);
            CT lat2 = geometry::get_as_radian<1>(p1);

            CT vertex_lat;
            CT lat_sum = lat1 + lat2;
            if (lat_sum > CT(0))
            {
                vertex_lat = geometry::get_as_radian<geometry::max_corner, 1>(mbr);
            } else {
                vertex_lat = geometry::get_as_radian<geometry::min_corner, 1>(mbr);
            }

            CT alp1;
            az_strategy.apply(lon1, lat1, lon2, lat2, alp1);
            CT vertex_lon = geometry::formula::vertex_longitude
                    <
                    CT,
                    segment_cs_type
                    >::apply(lon1, lat1, lon2, lat2,
                             vertex_lat, alp1, az_strategy);

            geometry::set_from_radian<0>(p_max, vertex_lon);
            geometry::set_from_radian<1>(p_max, vertex_lat);
        }
        //otherwise disjoint and vertex computed inside disjoint

        if (less_equal(geometry::get_as_radian<0>(bottom_left),
                       geometry::get_as_radian<0>(p_max)))
        {
            result = boost::numeric_cast<ReturnType>(typename
                        SegmentBoxStrategy::distance_ps_strategy::type().apply(bottom_left, p0, p1));
        }
        else
        {
            result = geometry::detail::distance::segment_to_box_2D
                    <
                        ReturnType,
                        SegmentPoint,
                        BoxPoint,
                        SegmentBoxStrategy
                    >::template call_above_of_box
                    <
                    typename LessEqual::other
                    >(p1, p0, p_max, bottom_right, sb_strategy);
        }
        return result;
    }

    template <typename SPoint, typename BPoint>
    static void mirror(SPoint& p0,
                       SPoint& p1,
                       BPoint& bottom_left,
                       BPoint& bottom_right,
                       BPoint& top_left,
                       BPoint& top_right)
    {
        //if segment's vertex is the southest point then mirror geometries
        if (geometry::get<1>(p0) + geometry::get<1>(p1) < 0)
        {
            BPoint bl = bottom_left;
            BPoint br = bottom_right;
            geometry::set<1>(p0, geometry::get<1>(p0) * -1);
            geometry::set<1>(p1, geometry::get<1>(p1) * -1);
            geometry::set<1>(bottom_left, geometry::get<1>(top_left) * -1);
            geometry::set<1>(top_left, geometry::get<1>(bl) * -1);
            geometry::set<1>(bottom_right, geometry::get<1>(top_right) * -1);
            geometry::set<1>(top_right, geometry::get<1>(br) * -1);
        }
    }
};

//===========================================================================

template
<
    typename CalculationType = void,
    typename Strategy = haversine<double, CalculationType>
>
struct spherical_segment_box
{
    template <typename PointOfSegment, typename PointOfBox>
    struct calculation_type
        : promote_floating_point
          <
              typename strategy::distance::services::return_type
                  <
                      Strategy,
                      PointOfSegment,
                      PointOfBox
                  >::type
          >
    {};

    // strategy getters

    // point-point strategy getters
    struct distance_pp_strategy
    {
        typedef Strategy type;
    };

    inline typename distance_pp_strategy::type get_distance_pp_strategy() const
    {
        return typename distance_pp_strategy::type();
    }
    // point-segment strategy getters
    struct distance_ps_strategy
    {
        typedef cross_track<CalculationType, Strategy> type;
    };

    inline typename distance_ps_strategy::type get_distance_ps_strategy() const
    {
        return typename distance_ps_strategy::type();
    }

    // methods

    template <typename LessEqual, typename ReturnType,
              typename SegmentPoint, typename BoxPoint>
    inline ReturnType segment_below_of_box(SegmentPoint const& p0,
                                   SegmentPoint const& p1,
                                   BoxPoint const& top_left,
                                   BoxPoint const& top_right,
                                   BoxPoint const& bottom_left,
                                   BoxPoint const& bottom_right) const
    {
        typedef typename azimuth::spherical<CalculationType> azimuth_strategy_type;
        azimuth_strategy_type az_strategy;

        typedef typename envelope::spherical_segment<CalculationType>
                                             envelope_segment_strategy_type;
        envelope_segment_strategy_type es_strategy;

        return generic_segment_box::segment_below_of_box
               <
                    LessEqual,
                    ReturnType
               >(p0,p1,top_left,top_right,bottom_left,bottom_right,
                 spherical_segment_box<CalculationType>(),
                 az_strategy, es_strategy);
    }

    template <typename SPoint, typename BPoint>
    static void mirror(SPoint& p0,
                       SPoint& p1,
                       BPoint& bottom_left,
                       BPoint& bottom_right,
                       BPoint& top_left,
                       BPoint& top_right)
    {

       generic_segment_box::mirror(p0, p1,
                                   bottom_left, bottom_right,
                                   top_left, top_right);
    }

};

#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS
namespace services
{

template <typename CalculationType, typename Strategy>
struct tag<spherical_segment_box<CalculationType, Strategy> >
{
    typedef strategy_tag_distance_segment_box type;
};

template <typename CalculationType, typename Strategy, typename PS, typename PB>
struct return_type<spherical_segment_box<CalculationType, Strategy>, PS, PB>
    : spherical_segment_box<CalculationType, Strategy>::template calculation_type<PS, PB>
{};

template <typename CalculationType, typename Strategy>
struct comparable_type<spherical_segment_box<CalculationType, Strategy> >
{
    // Define a cartesian_segment_box strategy with its underlying point-segment
    // strategy being comparable
    typedef spherical_segment_box
        <
            CalculationType,
            typename comparable_type<Strategy>::type
        > type;
};


template <typename CalculationType, typename Strategy>
struct get_comparable<spherical_segment_box<CalculationType, Strategy> >
{
    typedef typename comparable_type
        <
            spherical_segment_box<CalculationType, Strategy>
        >::type comparable_type;
public :
    static inline comparable_type apply(spherical_segment_box<CalculationType, Strategy> const& )
    {
        return comparable_type();
    }
};

template <typename CalculationType, typename Strategy, typename PS, typename PB>
struct result_from_distance<spherical_segment_box<CalculationType, Strategy>, PS, PB>
{
private :
    typedef typename return_type<
                                    spherical_segment_box
                                    <
                                        CalculationType,
                                        Strategy
                                    >,
                                    PS,
                                    PB
                                 >::type return_type;
public :
    template <typename T>
    static inline return_type apply(spherical_segment_box<CalculationType,
                                                          Strategy> const& ,
                                    T const& value)
    {
        Strategy s;
        return result_from_distance<Strategy, PS, PB>::apply(s, value);
    }
};

template <typename Segment, typename Box>
struct default_strategy
    <
        segment_tag, box_tag, Segment, Box,
        spherical_equatorial_tag, spherical_equatorial_tag
    >
{
    typedef spherical_segment_box<> type;
};

template <typename Box, typename Segment>
struct default_strategy
    <
        box_tag, segment_tag, Box, Segment,
        spherical_equatorial_tag, spherical_equatorial_tag
    >
{
    typedef typename default_strategy
        <
            segment_tag, box_tag, Segment, Box,
            spherical_equatorial_tag, spherical_equatorial_tag
        >::type type;
};

}
#endif

}} // namespace strategy::distance

}} // namespace boost::geometry
#endif // BOOST_GEOMETRY_STRATEGIES_SPHERICAL_DISTANCE_SEGMENT_BOX_HPP
