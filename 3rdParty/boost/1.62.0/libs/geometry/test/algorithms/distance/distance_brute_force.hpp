// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_DISTANCE_BRUTE_FORCE_HPP
#define BOOST_GEOMETRY_TEST_DISTANCE_BRUTE_FORCE_HPP

#include <iterator>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/or.hpp>
#include <boost/range.hpp>

#include <boost/geometry/core/reverse_dispatch.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tag_cast.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/iterators/segment_iterator.hpp>

#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>


namespace boost { namespace geometry
{

namespace unit_test
{

namespace detail { namespace distance_brute_force
{

struct distance_from_bg
{
    template <typename G>
    struct use_distance_from_bg
    {
        typedef typename boost::mpl::or_
            <
                boost::is_same<typename tag<G>::type, point_tag>,
                typename boost::mpl::or_
                    <
                        boost::is_same<typename tag<G>::type, segment_tag>,
                        boost::is_same<typename tag<G>::type, box_tag>
                    >::type
            >::type type;
    };

    template <typename Geometry1, typename Geometry2, typename Strategy>
    static inline
    typename distance_result<Geometry1, Geometry2, Strategy>::type
    apply(Geometry1 const& geometry1,
          Geometry2 const& geometry2,
          Strategy const& strategy)
    {
        BOOST_MPL_ASSERT((typename use_distance_from_bg<Geometry1>::type));
        BOOST_MPL_ASSERT((typename use_distance_from_bg<Geometry2>::type));

        return geometry::distance(geometry1, geometry2, strategy);
    }
};


template <typename Geometry1, typename Geometry2, typename Strategy>
inline
typename distance_result<Geometry1, Geometry2, Strategy>::type
bg_distance(Geometry1 const& geometry1,
            Geometry2 const& geometry2,
            Strategy const& strategy)
{
    return distance_from_bg::apply(geometry1, geometry2, strategy);
}


template <typename Policy>
struct one_to_many
{
    template <typename Geometry, typename Iterator, typename Strategy>
    static inline typename distance_result
        <
            Geometry,
            typename std::iterator_traits<Iterator>::value_type,
            Strategy
        >::type
    apply(Geometry const& geometry, Iterator begin, Iterator end,
          Strategy const& strategy)
    {
        typedef typename distance_result
        <
            Geometry,
            typename std::iterator_traits<Iterator>::value_type,
            Strategy
        >::type distance_type;

        bool first = true;
        distance_type d_min(0);
        for (Iterator it = begin; it != end; ++it, first = false)
        {
            distance_type d = Policy::apply(geometry, *it, strategy);

            if ( first || d < d_min )
            {
                d_min = d;
            }
        }
        return d_min;
    }
};



}} // namespace detail::distance_brute_force


namespace dispatch
{

template
<
    typename Geometry1,
    typename Geometry2,
    typename Strategy,
    typename Tag1 = typename tag_cast
        <
            typename tag<Geometry1>::type,
            segment_tag,
            linear_tag
        >::type,
    typename Tag2 = typename tag_cast
        <
            typename tag<Geometry2>::type,
            segment_tag,
            linear_tag
        >::type,
    bool Reverse = reverse_dispatch<Geometry1, Geometry2>::type::value
>
struct distance_brute_force
    : not_implemented<Geometry1, Geometry2>
{};


template
<
    typename Geometry1,
    typename Geometry2,
    typename Strategy,
    typename Tag1,
    typename Tag2
>
struct distance_brute_force<Geometry1, Geometry2, Strategy, Tag1, Tag2, true>
{
    static inline typename distance_result<Geometry1, Geometry2, Strategy>::type
    apply(Geometry1 const& geometry1,
          Geometry2 const& geometry2,
          Strategy const& strategy)
    {
        return distance_brute_force
            <
                Geometry2, Geometry1, Strategy
            >::apply(geometry2, geometry1, strategy);
    }
};


template 
<
    typename Point1,
    typename Point2,
    typename Strategy
>
struct distance_brute_force
<
    Point1, Point2, Strategy,
    point_tag, point_tag, false
> : detail::distance_brute_force::distance_from_bg
{};


template 
<
    typename Point,
    typename Segment,
    typename Strategy
>
struct distance_brute_force
<
    Point, Segment, Strategy,
    point_tag, segment_tag, false
> : detail::distance_brute_force::distance_from_bg
{};


template 
<
    typename Point,
    typename Box,
    typename Strategy
>
struct distance_brute_force
<
    Point, Box, Strategy,
    point_tag, box_tag, false
> : detail::distance_brute_force::distance_from_bg
{};


template
<
    typename Segment1,
    typename Segment2,
    typename Strategy
>
struct distance_brute_force
<
    Segment1, Segment2, Strategy,
    segment_tag, segment_tag, false
> : detail::distance_brute_force::distance_from_bg
{};


template 
<
    typename Point,
    typename Linear,
    typename Strategy
>
struct distance_brute_force
<
    Point, Linear, Strategy,
    point_tag, linear_tag, false
>
{
    typedef typename distance_result
        <
            Point, Linear, Strategy
        >::type distance_type;

    static inline distance_type apply(Point const& point,
                                      Linear const& linear,
                                      Strategy const& strategy)
    {
        return detail::distance_brute_force::one_to_many
            <
                detail::distance_brute_force::distance_from_bg
            >::apply(point,
                     geometry::segments_begin(linear),
                     geometry::segments_end(linear),
                     strategy);
    }
};


template 
<
    typename Point,
    typename MultiPoint,
    typename Strategy
>
struct distance_brute_force
<
    Point, MultiPoint, Strategy,
    point_tag, multi_point_tag, false
>
{
    typedef typename distance_result
        <
            Point, MultiPoint, Strategy
        >::type distance_type;

    static inline distance_type apply(Point const& p,
                                      MultiPoint const& mp,
                                      Strategy const& strategy)
    {
        return detail::distance_brute_force::one_to_many
            <
                detail::distance_brute_force::distance_from_bg
            >::apply(p, boost::begin(mp), boost::end(mp), strategy);
    }
};

template 
<
    typename MultiPoint1,
    typename MultiPoint2,
    typename Strategy
>
struct distance_brute_force
<
    MultiPoint1, MultiPoint2, Strategy,
    multi_point_tag, multi_point_tag, false
>
{
    typedef typename distance_result
        <
            MultiPoint1, MultiPoint2, Strategy
        >::type distance_type;

    static inline distance_type apply(MultiPoint1 const& mp1,
                                      MultiPoint2 const& mp2,
                                      Strategy const& strategy)
    {
        return detail::distance_brute_force::one_to_many
            <
                distance_brute_force
                <
                    MultiPoint1,
                    typename boost::range_value<MultiPoint2>::type,
                    Strategy
                >
            >::apply(mp1, boost::begin(mp2), boost::end(mp2), strategy);
    }
};


template 
<
    typename MultiPoint,
    typename Linear,
    typename Strategy
>
struct distance_brute_force
<
    MultiPoint, Linear, Strategy,
    multi_point_tag, linear_tag, false
>
{
    typedef typename distance_result
        <
            MultiPoint, Linear, Strategy
        >::type distance_type;

    static inline distance_type apply(MultiPoint const& mp,
                                      Linear const& linear,
                                      Strategy const& strategy)
    {
        return detail::distance_brute_force::one_to_many
            <
                distance_brute_force
                <
                    Linear,
                    typename boost::range_value<MultiPoint>::type,
                    Strategy
                >
            >::apply(linear, boost::begin(mp), boost::end(mp), strategy);
    }
};


template 
<
    typename Linear,
    typename MultiPoint,
    typename Strategy
>
struct distance_brute_force
<
    Linear, MultiPoint, Strategy,
    linear_tag, multi_point_tag, false
>
{
    typedef typename distance_result
        <
            Linear, MultiPoint, Strategy
        >::type distance_type;

    static inline distance_type apply(Linear const& linear,
                                      MultiPoint const& multipoint,
                                      Strategy const& strategy)
    {
        return distance_brute_force
            <
                MultiPoint, Linear, Strategy,
                multi_point_tag, linear_tag, false
            >::apply(multipoint, linear, strategy);
    }
};


template 
<
    typename MultiPoint,
    typename Segment,
    typename Strategy
>
struct distance_brute_force
<
    MultiPoint, Segment, Strategy,
    multi_point_tag, segment_tag, false
>
{
    typedef typename distance_result
        <
            MultiPoint, Segment, Strategy
        >::type distance_type;

    static inline distance_type apply(MultiPoint const& mp,
                                      Segment const& segment,
                                      Strategy const& strategy)
    {
        return detail::distance_brute_force::one_to_many
            <
                detail::distance_brute_force::distance_from_bg
            >::apply(segment, boost::begin(mp), boost::end(mp), strategy);
    }
};


template 
<
    typename Linear,
    typename Segment,
    typename Strategy
>
struct distance_brute_force
<
    Linear, Segment, Strategy,
    linear_tag, segment_tag, false
>
{
    typedef typename distance_result
        <
            Linear, Segment, Strategy
        >::type distance_type;

    static inline distance_type apply(Linear const& linear,
                                      Segment const& segment,
                                      Strategy const& strategy)
    {
        return detail::distance_brute_force::one_to_many
            <
                detail::distance_brute_force::distance_from_bg
            >::apply(segment,
                     geometry::segments_begin(linear),
                     geometry::segments_end(linear),
                     strategy);
    }
};


template 
<
    typename Linear1,
    typename Linear2,
    typename Strategy
>
struct distance_brute_force
<
    Linear1, Linear2, Strategy,
    linear_tag, linear_tag, false
>
{
    typedef typename distance_result
        <
            Linear1, Linear2, Strategy
        >::type distance_type;

    static inline distance_type apply(Linear1 const& linear1,
                                      Linear2 const& linear2,
                                      Strategy const& strategy)
    {
        return detail::distance_brute_force::one_to_many
            <
                distance_brute_force
                    <
                        Linear1,
                        typename std::iterator_traits
                            <
                                segment_iterator<Linear2 const>
                            >::value_type,
                        Strategy
                    >
            >::apply(linear1,
                     geometry::segments_begin(linear2),
                     geometry::segments_end(linear2),
                     strategy);
    }
};

} // namespace dispatch





template <typename Geometry1, typename Geometry2, typename Strategy>
inline typename distance_result<Geometry1, Geometry2, Strategy>::type
distance_brute_force(Geometry1 const& geometry1,
                     Geometry2 const& geometry2,
                     Strategy const& strategy)
{
    return dispatch::distance_brute_force
        <
            Geometry1, Geometry2, Strategy
        >::apply(geometry1, geometry2, strategy);
}

} // namespace unit_test


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_TEST_DISTANCE_BRUTE_FORCE_HPP
