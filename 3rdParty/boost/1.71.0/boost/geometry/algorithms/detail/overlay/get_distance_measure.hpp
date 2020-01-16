// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2019 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_DISTANCE_MEASURE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_DISTANCE_MEASURE_HPP

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_system.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/util/select_coordinate_type.hpp>

#include <cmath>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

template <typename T>
struct distance_measure
{
    T measure;

    distance_measure()
        : measure(T())
    {}

    bool is_small() const { return true; }
    bool is_zero() const { return true; }
    bool is_positive() const { return false; }
    bool is_negative() const { return false; }
};

template <typename T>
struct distance_measure_floating
{
    T measure;

    distance_measure_floating()
        : measure(T())
    {}

    // Returns true if the distance measure is small.
    // This is an arbitrary boundary, to enable some behaviour
    // (for example include or exclude turns), which are checked later
    // with other conditions.
    bool is_small() const { return std::abs(measure) < 1.0e-3; }

    // Returns true if the distance measure is absolutely zero
    bool is_zero() const { return measure == 0.0; }

    // Returns true if the distance measure is positive. Distance measure
    // algorithm returns positive value if it is located on the left side.
    bool is_positive() const { return measure > 0.0; }

    // Returns true if the distance measure is negative. Distance measure
    // algorithm returns negative value if it is located on the right side.
    bool is_negative() const { return measure < 0.0; }
};

template <>
struct distance_measure<long double>
    : public distance_measure_floating<long double> {};

template <>
struct distance_measure<double>
    : public distance_measure_floating<double> {};

template <>
struct distance_measure<float>
    : public distance_measure_floating<float> {};

} // detail


namespace detail_dispatch
{

// TODO: this is effectively a strategy, but for internal usage.
// It might be moved to the strategies folder.

template <typename CalculationType, typename CsTag>
struct get_distance_measure
        : not_implemented<CsTag>
{};

template <typename CalculationType>
struct get_distance_measure<CalculationType, cartesian_tag>
{
    typedef detail::distance_measure<CalculationType> result_type;

    template <typename SegmentPoint, typename Point>
    static result_type apply(SegmentPoint const& p1, SegmentPoint const& p2,
                             Point const& p)
    {
        typedef CalculationType ct;

        // Construct a line in general form (ax + by + c = 0),
        // (will be replaced by a general_form structure in next PR)
        ct const x1 = geometry::get<0>(p1);
        ct const y1 = geometry::get<1>(p1);
        ct const x2 = geometry::get<0>(p2);
        ct const y2 = geometry::get<1>(p2);
        ct const a = y1 - y2;
        ct const b = x2 - x1;
        ct const c = -a * x1 - b * y1;

        // Returns a distance measure
        // https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line#Line_defined_by_an_equation
        // dividing by sqrt(a*a+b*b) is not necessary for this distance measure,
        // it is not a real distance and purpose is to detect small differences
        // in collinearity
        result_type result;
        result.measure = a * geometry::get<0>(p) + b * geometry::get<1>(p) + c;

        return result;
    }
};

template <typename CalculationType>
struct get_distance_measure<CalculationType, spherical_tag>
{
    typedef detail::distance_measure<CalculationType> result_type;

    template <typename SegmentPoint, typename Point>
    static result_type apply(SegmentPoint const& , SegmentPoint const& ,
                             Point const& )
    {
        // TODO, optional
        result_type result;
        return result;
    }
};

template <typename CalculationType>
struct get_distance_measure<CalculationType, geographic_tag>
        : get_distance_measure<CalculationType, spherical_tag> {};


} // namespace detail_dispatch

namespace detail
{

// Returns a (often very tiny) value to indicate its side, and distance,
// 0 (absolutely 0, not even an epsilon) means collinear. Like side,
// a negative means that p is to the right of p1-p2. And a positive value
// means that p is to the left of p1-p2.

template <typename cs_tag, typename SegmentPoint, typename Point>
static distance_measure<typename select_coordinate_type<SegmentPoint, Point>::type>
get_distance_measure(SegmentPoint const& p1, SegmentPoint const& p2, Point const& p)
{
    return detail_dispatch::get_distance_measure
            <
                typename select_coordinate_type<SegmentPoint, Point>::type,
                cs_tag
            >::apply(p1, p2, p);

}

} // namespace detail
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_DISTANCE_MEASURE_HPP
