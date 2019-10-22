// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2018-2019 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fisikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_STRATEGIES_CARTESIAN_DISTANCE_SEGMENT_BOX_HPP
#define BOOST_GEOMETRY_STRATEGIES_CARTESIAN_DISTANCE_SEGMENT_BOX_HPP

#include <boost/geometry/algorithms/detail/distance/segment_to_box.hpp>

#include <boost/geometry/strategies/cartesian/distance_projected_point.hpp>
#include <boost/geometry/strategies/cartesian/distance_pythagoras.hpp>
#include <boost/geometry/strategies/cartesian/distance_pythagoras_point_box.hpp>
#include <boost/geometry/strategies/cartesian/point_in_point.hpp>
#include <boost/geometry/strategies/cartesian/side_by_triangle.hpp>

namespace boost { namespace geometry
{


namespace strategy { namespace distance
{

template
<
    typename CalculationType = void,
    typename Strategy = pythagoras<CalculationType>
>
struct cartesian_segment_box
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

    typedef cartesian_tag cs_tag;

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
        typedef projected_point<CalculationType, Strategy> type;
    };

    inline typename distance_ps_strategy::type get_distance_ps_strategy() const
    {
        return typename distance_ps_strategy::type();
    }

    struct distance_pb_strategy
    {
        typedef pythagoras_point_box<CalculationType> type;
    };

    inline typename distance_pb_strategy::type get_distance_pb_strategy() const
    {
        return typename distance_pb_strategy::type();
    }

    typedef side::side_by_triangle<CalculationType> side_strategy_type;

    static inline side_strategy_type get_side_strategy()
    {
        return side_strategy_type();
    }

    typedef within::cartesian_point_point equals_point_point_strategy_type;

    static inline equals_point_point_strategy_type get_equals_point_point_strategy()
    {
        return equals_point_point_strategy_type();
    }

    template <typename LessEqual, typename ReturnType,
              typename SegmentPoint, typename BoxPoint>
    inline ReturnType segment_below_of_box(SegmentPoint const& p0,
                                   SegmentPoint const& p1,
                                   BoxPoint const&,
                                   BoxPoint const&,
                                   BoxPoint const&,
                                   BoxPoint const& bottom_right) const
    {


        return geometry::detail::distance::segment_to_box_2D
            <
                ReturnType,
                SegmentPoint,
                BoxPoint,
                cartesian_segment_box<CalculationType, Strategy>
            >::template call_above_of_box
                <
                    typename LessEqual::other
                >(p1, p0, bottom_right, *this);
    }

    template <typename SPoint, typename BPoint>
    static void mirror(SPoint&,
                       SPoint&,
                       BPoint&,
                       BPoint&,
                       BPoint&,
                       BPoint&)
    {}
};

#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS
namespace services
{

template <typename CalculationType, typename Strategy>
struct tag<cartesian_segment_box<CalculationType, Strategy> >
{
    typedef strategy_tag_distance_segment_box type;
};

template <typename CalculationType, typename Strategy, typename PS, typename PB>
struct return_type<cartesian_segment_box<CalculationType, Strategy>, PS, PB>
    : cartesian_segment_box<CalculationType, Strategy>::template calculation_type<PS, PB>
{};

template <typename CalculationType, typename Strategy>
struct comparable_type<cartesian_segment_box<CalculationType, Strategy> >
{
    // Define a cartesian_segment_box strategy with its underlying point-point
    // strategy being comparable
    typedef cartesian_segment_box
        <
            CalculationType,
            typename comparable_type<Strategy>::type
        > type;
};


template <typename CalculationType, typename Strategy>
struct get_comparable<cartesian_segment_box<CalculationType, Strategy> >
{
    typedef typename comparable_type
        <
            cartesian_segment_box<CalculationType, Strategy>
        >::type comparable_type;
public :
    static inline comparable_type apply(cartesian_segment_box<CalculationType, Strategy> const& )
    {
        return comparable_type();
    }
};

template <typename CalculationType, typename Strategy, typename PS, typename PB>
struct result_from_distance<cartesian_segment_box<CalculationType, Strategy>, PS, PB>
{
private :
    typedef typename return_type<
                                    cartesian_segment_box
                                    <
                                        CalculationType,
                                        Strategy
                                    >,
                                    PS,
                                    PB
                                 >::type return_type;
public :
    template <typename T>
    static inline return_type apply(cartesian_segment_box<CalculationType,
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
        cartesian_tag, cartesian_tag
    >
{
    typedef cartesian_segment_box<> type;
};

template <typename Box, typename Segment>
struct default_strategy
    <
        box_tag, segment_tag, Box, Segment,
        cartesian_tag, cartesian_tag
    >
{
    typedef typename default_strategy
        <
            segment_tag, box_tag, Segment, Box,
            cartesian_tag, cartesian_tag
        >::type type;
};

}
#endif

}} // namespace strategy::distance

}} // namespace boost::geometry
#endif // BOOST_GEOMETRY_STRATEGIES_CARTESIAN_DISTANCE_SEGMENT_BOX_HPP
