// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2014-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2014-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2014-2015 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2015, 2019.
// Modifications copyright (c) 2015, 2019, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_POLICIES_ROBUSTNESS_GET_RESCALE_POLICY_HPP
#define BOOST_GEOMETRY_POLICIES_ROBUSTNESS_GET_RESCALE_POLICY_HPP


#include <cstddef>

#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_floating_point.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/config.hpp>
#include <boost/geometry/core/tag_cast.hpp>

#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/expand.hpp>
#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/detail/recalculate.hpp>
#include <boost/geometry/algorithms/detail/get_max_size.hpp>
#include <boost/geometry/policies/robustness/robust_type.hpp>

#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>

#include <boost/geometry/policies/robustness/no_rescale_policy.hpp>
#include <boost/geometry/policies/robustness/rescale_policy.hpp>

#include <boost/geometry/util/promote_floating_point.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace get_rescale_policy
{

template
<
    typename Box,
    typename Point,
    typename RobustPoint,
    typename Factor
>
inline void scale_box_to_integer_range(Box const& box,
                                       Point& min_point,
                                       RobustPoint& min_robust_point,
                                       Factor& factor)
{
    // Scale box to integer-range
    typedef typename promote_floating_point
        <
            typename geometry::coordinate_type<Point>::type
        >::type num_type;
    num_type const diff = boost::numeric_cast<num_type>(detail::get_max_size(box));
    num_type const range = 10000000.0; // Define a large range to get precise integer coordinates
    num_type const half = 0.5;
    if (math::equals(diff, num_type())
        || diff >= range
        || ! boost::math::isfinite(diff))
    {
        factor = 1;
    }
    else
    {
        factor = boost::numeric_cast<num_type>(
            boost::numeric_cast<boost::long_long_type>(half + range / diff));
        BOOST_GEOMETRY_ASSERT(factor >= 1);
    }

    // Assign input/output minimal points
    detail::assign_point_from_index<0>(box, min_point);
    num_type const two = 2;
    boost::long_long_type const min_coordinate
        = boost::numeric_cast<boost::long_long_type>(-range / two);
    assign_values(min_robust_point, min_coordinate, min_coordinate);
}

template
<
    typename Point, typename RobustPoint, typename Geometry,
    typename Factor, typename EnvelopeStrategy
>
static inline void init_rescale_policy(Geometry const& geometry,
        Point& min_point,
        RobustPoint& min_robust_point,
        Factor& factor,
        EnvelopeStrategy const& strategy)
{
    if (geometry::is_empty(geometry))
    {
        return;
    }

    // Get bounding box
    model::box<Point> env = geometry::return_envelope
                                <
                                    model::box<Point>
                                >(geometry, strategy);

    scale_box_to_integer_range(env, min_point, min_robust_point, factor);
}

// NOTE: Actually it should take 2 separate strategies, one for each geometry
// in case one of them was e.g. a Box
template
<
    typename Point, typename RobustPoint, typename Geometry1, typename Geometry2,
    typename Factor, typename EnvelopeStrategy1, typename EnvelopeStrategy2
>
static inline void init_rescale_policy(Geometry1 const& geometry1,
        Geometry2 const& geometry2,
        Point& min_point,
        RobustPoint& min_robust_point,
        Factor& factor,
        EnvelopeStrategy1 const& strategy1,
        EnvelopeStrategy2 const& strategy2)
{
    // Get bounding boxes (when at least one of the geometries is not empty)
    bool const is_empty1 = geometry::is_empty(geometry1);
    bool const is_empty2 = geometry::is_empty(geometry2);
    if (is_empty1 && is_empty2)
    {
        return;
    }

    model::box<Point> env;
    if (is_empty1)
    {
        geometry::envelope(geometry2, env, strategy2);
    }
    else if (is_empty2)
    {
        geometry::envelope(geometry1, env, strategy1);
    }
    else
    {
        // The following approach (envelope + expand) may not give the
        // optimal MBR when then two geometries are in the spherical
        // equatorial or geographic coordinate systems.
        // TODO: implement envelope for two (or possibly more geometries)
        geometry::envelope(geometry1, env, strategy1);
        model::box<Point> env2 = geometry::return_envelope
            <
                model::box<Point>
            >(geometry2, strategy2);
        geometry::expand(env, env2, strategy1.get_box_expand_strategy());
    }

    scale_box_to_integer_range(env, min_point, min_robust_point, factor);
}


template
<
    typename Point,
    bool IsFloatingPoint
>
struct rescale_policy_type
{
    typedef no_rescale_policy type;
};

// We rescale only all FP types
template
<
    typename Point
>
struct rescale_policy_type<Point, true>
{
    typedef typename geometry::coordinate_type<Point>::type coordinate_type;
    typedef model::point
    <
        typename detail::robust_type<coordinate_type>::type,
        geometry::dimension<Point>::value,
        typename geometry::coordinate_system<Point>::type
    > robust_point_type;
    typedef typename promote_floating_point<coordinate_type>::type factor_type;
    typedef detail::robust_policy<Point, robust_point_type, factor_type> type;
};

template <typename Policy>
struct get_rescale_policy
{
    template <typename Geometry, typename EnvelopeStrategy>
    static inline Policy apply(Geometry const& geometry,
                               EnvelopeStrategy const& strategy)
    {
        typedef typename point_type<Geometry>::type point_type;
        typedef typename geometry::coordinate_type<Geometry>::type coordinate_type;
        typedef typename promote_floating_point<coordinate_type>::type factor_type;
        typedef model::point
        <
            typename detail::robust_type<coordinate_type>::type,
            geometry::dimension<point_type>::value,
            typename geometry::coordinate_system<point_type>::type
        > robust_point_type;

        point_type min_point;
        robust_point_type min_robust_point;
        factor_type factor;
        init_rescale_policy(geometry, min_point, min_robust_point,
                            factor, strategy);

        return Policy(min_point, min_robust_point, factor);
    }

    template <typename Geometry1, typename Geometry2, typename EnvelopeStrategy1, typename EnvelopeStrategy2>
    static inline Policy apply(Geometry1 const& geometry1, Geometry2 const& geometry2,
                               EnvelopeStrategy1 const& strategy1,
                               EnvelopeStrategy2 const& strategy2)
    {
        typedef typename point_type<Geometry1>::type point_type;
        typedef typename geometry::coordinate_type<Geometry1>::type coordinate_type;
        typedef typename promote_floating_point<coordinate_type>::type factor_type;
        typedef model::point
        <
            typename detail::robust_type<coordinate_type>::type,
            geometry::dimension<point_type>::value,
            typename geometry::coordinate_system<point_type>::type
        > robust_point_type;

        point_type min_point;
        robust_point_type min_robust_point;
        factor_type factor;
        init_rescale_policy(geometry1, geometry2, min_point, min_robust_point,
                            factor, strategy1, strategy2);

        return Policy(min_point, min_robust_point, factor);
    }
};

// Specialization for no-rescaling
template <>
struct get_rescale_policy<no_rescale_policy>
{
    template <typename Geometry, typename EnvelopeStrategy>
    static inline no_rescale_policy apply(Geometry const& , EnvelopeStrategy const&)
    {
        return no_rescale_policy();
    }

    template <typename Geometry1, typename Geometry2, typename EnvelopeStrategy1, typename EnvelopeStrategy2>
    static inline no_rescale_policy apply(Geometry1 const& , Geometry2 const& ,
                                          EnvelopeStrategy1 const& , EnvelopeStrategy2 const& )
    {
        return no_rescale_policy();
    }
};


}} // namespace detail::get_rescale_policy
#endif // DOXYGEN_NO_DETAIL

template
<
    typename Point,
    typename CSTag = typename geometry::cs_tag<Point>::type
>
struct rescale_policy_type
    : public detail::get_rescale_policy::rescale_policy_type
    <
        Point,
#if defined(BOOST_GEOMETRY_USE_RESCALING)
        boost::is_floating_point
            <
                typename geometry::coordinate_type<Point>::type
            >::type::value
        &&
        boost::is_same
            <
                CSTag,
                geometry::cartesian_tag
            >::value
#else
        false
#endif
    >
{
    static const bool is_point
        = boost::is_same
            <
                typename geometry::tag<Point>::type,
                geometry::point_tag
            >::type::value;

    BOOST_MPL_ASSERT_MSG((is_point),
                         INVALID_INPUT_GEOMETRY,
                         (typename geometry::tag<Point>::type));
};


template
<
    typename Geometry1,
    typename Geometry2,
    typename CSTag = typename geometry::cs_tag<Geometry1>::type,
    typename Tag1 = typename tag_cast
    <
        typename tag<Geometry1>::type,
        box_tag,
        pointlike_tag,
        linear_tag,
        areal_tag
    >::type,
    typename Tag2 = typename tag_cast
    <
        typename tag<Geometry2>::type,
        box_tag,
        pointlike_tag,
        linear_tag,
        areal_tag
    >::type
>
struct rescale_overlay_policy_type
    // Default: no rescaling
    : public detail::get_rescale_policy::rescale_policy_type
        <
            typename geometry::point_type<Geometry1>::type,
            false
        >
{};

// Areal/areal: get rescale policy based on coordinate type
template
<
    typename Geometry1,
    typename Geometry2,
    typename CSTag
>
struct rescale_overlay_policy_type<Geometry1, Geometry2, CSTag, areal_tag, areal_tag>
    : public rescale_policy_type
        <
            typename geometry::point_type<Geometry1>::type,
            CSTag
        >
{};


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace get_rescale_policy
{


// get envelope strategy compatible with relate strategy based on geometry tag
// and strategy cs_tag
template
<
    typename Geometry,
    typename Strategy,
    typename Tag = typename geometry::tag<Geometry>::type,
    typename CSTag = typename Strategy::cs_tag
>
struct get_envelope_strategy
{
    typedef typename Strategy::envelope_strategy_type type;

    static inline type apply(Strategy const& strategy)
    {
        return strategy.get_envelope_strategy();
    }
};

template <typename Geometry, typename Strategy, typename CSTag>
struct get_envelope_strategy<Geometry, Strategy, box_tag, CSTag>
{
    typedef typename Strategy::envelope_box_strategy_type type;

    static inline type apply(Strategy const& )
    {
        return type();
    }
};

// NOTE: within::xxx_point_point shouldn't have a getter for envelope strategy
// so dispatch by CStag. In the future strategies should probably be redesigned.
template <typename Geometry, typename Strategy>
struct get_envelope_strategy<Geometry, Strategy, point_tag, cartesian_tag>
{
    typedef strategy::envelope::cartesian_point type;

    static inline type apply(Strategy const& )
    {
        return type();
    }
};
template <typename Geometry, typename Strategy>
struct get_envelope_strategy<Geometry, Strategy, point_tag, spherical_tag>
{
    typedef strategy::envelope::spherical_point type;

    static inline type apply(Strategy const& )
    {
        return type();
    }
};

template <typename Geometry, typename Strategy>
struct get_envelope_strategy<Geometry, Strategy, multi_point_tag, cartesian_tag>
{
    typedef strategy::envelope::cartesian_point type;

    static inline type apply(Strategy const& )
    {
        return type();
    }
};
template <typename Geometry, typename Strategy>
struct get_envelope_strategy<Geometry, Strategy, multi_point_tag, spherical_tag>
{
    typedef strategy::envelope::spherical_point type;

    static inline type apply(Strategy const& )
    {
        return type();
    }
};


// utility for backward-compatibility either treating the argument as geometry
// or envelope strategy for get_rescale_policy
template
<
    typename Geometry2OrStrategy,
    typename Tag = typename geometry::tag<Geometry2OrStrategy>::type
>
struct get_rescale_policy_geometry_or_strategy
{
    template <typename Policy, typename Geometry>
    static inline Policy apply(Geometry const& geometry, Geometry2OrStrategy const& geometry2)
    {
        typename geometry::strategy::envelope::services::default_strategy
            <
                typename geometry::tag<Geometry>::type,
                typename geometry::cs_tag<Geometry>::type
            >::type strategy1;
        typename geometry::strategy::envelope::services::default_strategy
            <
                typename geometry::tag<Geometry2OrStrategy>::type,
                typename geometry::cs_tag<Geometry2OrStrategy>::type
            >::type strategy2;

        return detail::get_rescale_policy::get_rescale_policy
            <
                Policy
            >::apply(geometry, geometry2, strategy1, strategy2);
    }
};

template <typename Strategy>
struct get_rescale_policy_geometry_or_strategy<Strategy, void>
{
    template <typename Policy, typename Geometry>
    static inline Policy apply(Geometry const& geometry, Strategy const& strategy)
    {
        return detail::get_rescale_policy::get_rescale_policy
            <
                Policy
            >::apply(geometry,
                     get_envelope_strategy
                        <
                            Geometry, Strategy
                        >::apply(strategy));
    }
};


}} // namespace detail::get_rescale_policy
#endif // DOXYGEN_NO_DETAIL


template <typename Policy, typename Geometry>
inline Policy get_rescale_policy(Geometry const& geometry)
{
    typename geometry::strategy::envelope::services::default_strategy
        <
            typename geometry::tag<Geometry>::type,
            typename geometry::cs_tag<Geometry>::type
        >::type strategy;

    return detail::get_rescale_policy::get_rescale_policy<Policy>::apply(geometry, strategy);
}

template <typename Policy, typename Geometry, typename Geometry2OrStrategy>
inline Policy get_rescale_policy(Geometry const& geometry, Geometry2OrStrategy const& geometry2_or_strategy)
{
    // if the second argument is a geometry use default strategy
    // otherwise assume it's envelope strategy for the first argument
    return detail::get_rescale_policy::get_rescale_policy_geometry_or_strategy
        <
            Geometry2OrStrategy
        > ::template apply<Policy, Geometry>(geometry, geometry2_or_strategy);
}

template <typename Policy, typename Geometry1, typename Geometry2, typename IntersectionStrategy>
inline Policy get_rescale_policy(Geometry1 const& geometry1, Geometry2 const& geometry2,
                                 IntersectionStrategy const& strategy)
{
    return detail::get_rescale_policy::get_rescale_policy
            <
                Policy
            >::apply(geometry1, geometry2,
                     detail::get_rescale_policy::get_envelope_strategy
                        <
                            Geometry1, IntersectionStrategy
                        >::apply(strategy),
                     detail::get_rescale_policy::get_envelope_strategy
                        <
                            Geometry2, IntersectionStrategy
                        >::apply(strategy));
}


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_POLICIES_ROBUSTNESS_GET_RESCALE_POLICY_HPP
