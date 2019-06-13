// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015-2017, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_ENVELOPE_EXPAND_ON_SPHEROID_HPP
#define BOOST_GEOMETRY_TEST_ENVELOPE_EXPAND_ON_SPHEROID_HPP


#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>

#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/cs.hpp>

#include <boost/geometry/util/condition.hpp>
#include <boost/geometry/util/math.hpp>

#include <boost/geometry/views/detail/indexed_point_view.hpp>

#include <boost/geometry/algorithms/assign.hpp>


struct rng
{
    typedef double type;

    rng(double l, double h)
        : lo(l), hi(h)
    {
        BOOST_GEOMETRY_ASSERT(lo <= hi);
    }

    friend rng operator*(rng const& l, double v) { return rng(l.lo * v, l.hi * v); }

    friend bool operator<=(rng const& l, rng const& r) { return l.lo <= r.hi; }
    friend bool operator<=(double l, rng const& r) { return l <= r.hi; }
    friend bool operator<=(rng const& l, double r) { return l.lo <= r; }

    friend bool operator<(rng const& l, rng const& r) { return !operator<=(r, l); }
    friend bool operator<(double l, rng const& r) { return !operator<=(r, l); }
    friend bool operator<(rng const& l, double r) { return !operator<=(r, l); }

    friend bool operator==(double l, rng const& r) { return r.lo <= l && l <= r.hi; }

    friend std::ostream & operator<<(std::ostream & os, rng const& v)
    {
        return (os << "[" << v.lo << ", " << v.hi << "]");
    }

    double lo, hi;
};


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

    static inline double convert(double value)
    {
        return value * bg::math::r2d<double>();
    }

    static inline rng convert(rng const& value)
    {
        return value * bg::math::r2d<double>();
    }
};

template <>
struct other_system_info<bg::cs::spherical_equatorial<bg::degree> >
{
    typedef bg::radian units;
    typedef bg::cs::spherical_equatorial<units> type;

    static inline double convert(double value)
    {
        return value * bg::math::d2r<double>();
    }

    static inline rng convert(rng const& value)
    {
        return value * bg::math::d2r<double>();
    }
};

template <>
struct other_system_info<bg::cs::spherical<bg::radian> >
{
    typedef bg::degree units;
    typedef bg::cs::spherical<units> type;

    static inline double convert(double value)
    {
        return value * bg::math::r2d<double>();
    }

    static inline rng convert(rng const& value)
    {
        return value * bg::math::r2d<double>();
    }
};

template <>
struct other_system_info<bg::cs::spherical<bg::degree> >
{
    typedef bg::radian units;
    typedef bg::cs::spherical<units> type;

    static inline double convert(double value)
    {
        return value * bg::math::d2r<double>();
    }

    static inline rng convert(rng const& value)
    {
        return value * bg::math::d2r<double>();
    }
};

template <>
struct other_system_info<bg::cs::geographic<bg::radian> >
{
    typedef bg::degree units;
    typedef bg::cs::geographic<units> type;

    static inline double convert(double value)
    {
        return value * bg::math::r2d<double>();
    }

    static inline rng convert(rng const& value)
    {
        return value * bg::math::r2d<double>();
    }
};

template <>
struct other_system_info<bg::cs::geographic<bg::degree> >
{
    typedef bg::radian units;
    typedef bg::cs::geographic<units> type;

    static inline double convert(double value)
    {
        return value * bg::math::d2r<double>();
    }

    static inline rng convert(rng const& value)
    {
        return value * bg::math::d2r<double>();
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

    inline bool operator()(double value1, double value2) const
    {
        return check_close(value1, value2, m_tolerence);
    }

    inline bool operator()(double l, rng const& r) const
    {
        return (r.lo < l && l < r.hi)
            || check_close(l, r.lo, m_tolerence)
            || check_close(l, r.hi, m_tolerence);
    }
};


bool equals_with_eps(double l, double r)
{
    return bg::math::equals(l, r);
}

bool equals_with_eps(double l, rng r)
{
    return (r.lo < l && l < r.hi)
        || bg::math::equals(l, r.lo)
        || bg::math::equals(l, r.hi);
}


template
<
    typename Box,
    std::size_t DimensionCount = bg::dimension<Box>::value
>
struct box_check_equals
{
    template <typename T1, typename T2, typename T3, typename T4>
    static inline bool apply(Box const& box,
                             T1 const& lon_min, T2 const& lat_min, double,
                             T3 const& lon_max, T4 const& lat_max, double,
                             double tol)
    {
        equals_with_tolerance equals(tol);

#ifndef BOOST_GEOMETRY_TEST_ENABLE_FAILING
        // check latitude with tolerance when necessary
        return equals_with_eps(bg::get<0, 0>(box), lon_min)
            && (bg::get<0, 1>(box) < 0
                ? equals(bg::get<0, 1>(box), lat_min)
                : equals_with_eps(bg::get<0, 1>(box), lat_min))
            && equals_with_eps(bg::get<1, 0>(box), lon_max)
            && (bg::get<1, 1>(box) > 0
                ? equals(bg::get<1, 1>(box), lat_max)
                : equals_with_eps(bg::get<1, 1>(box), lat_max));
#else
        // check latitude with tolerance when necessary
        return bg::get<0, 0>(box) == lon_min
            && (bg::get<0, 1>(box) < 0
                ? equals(bg::get<0, 1>(box), lat_min)
                : bg::get<0, 1>(box) == lat_min)
            && bg::get<1, 0>(box) == lon_max
            && (bg::get<1, 1>(box) > 0
                ? equals(bg::get<1, 1>(box), lat_max)
                : bg::get<1, 1>(box) == lat_max);
#endif
    }
};

template <typename Box>
struct box_check_equals<Box, 3>
{
    template <typename T1, typename T2, typename T3, typename T4>
    static inline bool apply(Box const& box,
                             T1 const& lon_min, T2 const& lat_min, double height_min,
                             T3 const& lon_max, T4 const& lat_max, double height_max,
                             double tol)
    {
#ifndef BOOST_GEOMETRY_TEST_ENABLE_FAILING
        equals_with_tolerance equals(tol);

        return box_check_equals<Box, 2>::apply(box,
                                               lon_min, lat_min, height_min,
                                               lon_max, lat_max, height_max,
                                               tol)
            && equals(bg::get<0, 2>(box), height_min)
            && equals(bg::get<1, 2>(box), height_max);
#else
        return box_equals<Box, 2>::apply(box,
                                         lon_min, lat_min, height_min,
                                         lon_max, lat_max, height_max,
                                         tol)
            && bg::get<0, 2>(box) == height_min
            && bg::get<1, 2>(box) == height_max;
#endif
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
        return box_check_equals<Box1>::apply(box1,
                    bg::get<0, 0>(box2), bg::get<0, 1>(box2), 0.0,
                    bg::get<1, 0>(box2), bg::get<1, 1>(box2), 0.0,
                    tol);
    }
};

template<typename Box1, typename Box2>
struct box_equals<Box1, Box2, 3>
{
    static inline bool apply(Box1 const& box1, Box2 const& box2, double tol)
    {
        return box_check_equals<Box1>::apply(box1,
                    bg::get<0, 0>(box2), bg::get<0, 1>(box2), bg::get<0, 2>(box2),
                    bg::get<1, 0>(box2), bg::get<1, 1>(box2), bg::get<1, 2>(box2),
                    tol);
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
