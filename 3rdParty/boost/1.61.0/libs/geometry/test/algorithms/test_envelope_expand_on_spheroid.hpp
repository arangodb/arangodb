// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_ENVELOPE_EXPAND_ON_SPHEROID_HPP
#define BOOST_GEOMETRY_TEST_ENVELOPE_EXPAND_ON_SPHEROID_HPP

#include <cmath>
#include <cstddef>
#include <algorithm>

#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/cs.hpp>

#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/util/math.hpp>

#include <boost/geometry/views/detail/indexed_point_view.hpp>

#include <boost/geometry/algorithms/assign.hpp>


template <typename Units>
char const* units2string()
{
    if (BOOST_GEOMETRY_CONDITION((boost::is_same<Units, bg::degree>::value)))
    {
        return "degrees";
    }
    return "radians";
}

template <typename CoordinateSystem>
struct other_system_info
{};

template <>
struct other_system_info<bg::cs::spherical_equatorial<bg::radian> >
{
    typedef bg::degree units;
    typedef bg::cs::spherical_equatorial<units> type;

    template <typename T>
    static inline T convert(T const& value)
    {
        return value * bg::math::r2d<T>();
    }
};

template <>
struct other_system_info<bg::cs::spherical_equatorial<bg::degree> >
{
    typedef bg::radian units;
    typedef bg::cs::spherical_equatorial<units> type;

    template <typename T>
    static inline T convert(T const& value)
    {
        return value * bg::math::d2r<T>();
    }
};

template <>
struct other_system_info<bg::cs::geographic<bg::radian> >
{
    typedef bg::degree units;
    typedef bg::cs::geographic<units> type;

    template <typename T>
    static inline T convert(T const& value)
    {
        return value * bg::math::r2d<T>();
    }
};

template <>
struct other_system_info<bg::cs::geographic<bg::degree> >
{
    typedef bg::radian units;
    typedef bg::cs::geographic<units> type;

    template <typename T>
    static inline T convert(T const& value)
    {
        return value * bg::math::d2r<T>();
    }
};



class equals_with_tolerance
{
private:
    double m_tolerence;

    template <typename T>
    static inline T const& get_max(T const& a, T const& b, T const& c)
    {
        return (std::max)((std::max)(a, b), c);
    }

    template <typename T>
    static inline bool check_close(T const& a, T const& b, double tol)
    {
        return (a == b)
            || (std::abs(a - b) <= tol * get_max(std::abs(a), std::abs(b), 1.0));
    }

public:
    equals_with_tolerance(double tolerence) : m_tolerence(tolerence) {}

    template <typename T>
    inline bool operator()(T const& value1, T const& value2) const
    {
        return check_close(value1, value2, m_tolerence);
    }
};


template
<
    typename Box1,
    typename Box2 = Box1,
    std::size_t DimensionCount = bg::dimension<Box1>::value
>
struct box_equals
{
    static inline bool apply(Box1 const& box1, Box2 const& box2, double tol)
    {
        equals_with_tolerance equals(tol);

#ifndef BOOST_GEOMETRY_TEST_ENABLE_FAILING
        return equals(bg::get<0, 0>(box1), bg::get<0, 0>(box2))
            && equals(bg::get<0, 1>(box1), bg::get<0, 1>(box2))
            && equals(bg::get<1, 0>(box1), bg::get<1, 0>(box2))
            && equals(bg::get<1, 1>(box1), bg::get<1, 1>(box2));
#else
        // check latitude with tolerance when necessary
        return bg::get<0, 0>(box1) == bg::get<0, 0>(box2)
            && (bg::get<0, 1>(box1) < 0
                ? equals(bg::get<0, 1>(box1), bg::get<0, 1>(box2))
                : bg::get<0, 1>(box1) == bg::get<0, 1>(box2))
            && bg::get<1, 0>(box1) == bg::get<1, 0>(box2)
            && (bg::get<1, 1>(box1) > 0
                ? equals(bg::get<1, 1>(box1), bg::get<1, 1>(box2))
                : bg::get<1, 1>(box1) == bg::get<1, 1>(box2));
#endif
    }
};

template <typename Box1, typename Box2>
struct box_equals<Box1, Box2, 3>
{
    static inline bool apply(Box1 const& box1, Box2 const& box2, double tol)
    {
#ifndef BOOST_GEOMETRY_TEST_ENABLE_FAILING
        equals_with_tolerance equals(tol);

        return box_equals<Box1, Box2, 2>::apply(box1, box2, tol)
            && equals(bg::get<0, 2>(box1), bg::get<0, 2>(box2))
            && equals(bg::get<1, 2>(box1), bg::get<1, 2>(box2));
#else
        return box_equals<Box1, Box2, 2>::apply(box1, box2, tol)
            && bg::get<0, 2>(box1) == bg::get<0, 2>(box2)
            && bg::get<1, 2>(box1) == bg::get<1, 2>(box2);
#endif
    }
};


template <typename Box, std::size_t Dimension = bg::dimension<Box>::value>
struct initialize_box
{
    static inline void apply(Box& box,
                             double lon_min, double lat_min, double,
                             double lon_max, double lat_max, double)
    {
        bg::detail::indexed_point_view<Box, bg::min_corner> p_min(box);
        bg::detail::indexed_point_view<Box, bg::max_corner> p_max(box);

        bg::assign_values(p_min, lon_min, lat_min);
        bg::assign_values(p_max, lon_max, lat_max);
    }
};

template <typename Box>
struct initialize_box<Box, 3>
{
    static inline void apply(Box& box,
                             double lon_min, double lat_min, double height_min,
                             double lon_max, double lat_max, double height_max)
    {
        bg::detail::indexed_point_view<Box, bg::min_corner> p_min(box);
        bg::detail::indexed_point_view<Box, bg::max_corner> p_max(box);

        bg::assign_values(p_min, lon_min, lat_min, height_min);
        bg::assign_values(p_max, lon_max, lat_max, height_max);
    }
};

#endif // BOOST_GEOMETRY_TEST_ENVELOPE_EXPAND_ON_SPHEROID_HPP
