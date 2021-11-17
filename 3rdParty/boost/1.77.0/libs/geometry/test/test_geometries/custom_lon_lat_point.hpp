// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_TEST_TEST_GEOMETRIES_CUSTOM_LON_LAT_POINT_HPP
#define BOOST_GEOMETRY_TEST_TEST_GEOMETRIES_CUSTOM_LON_LAT_POINT_HPP

#include <boost/mpl/int.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/coordinate_system.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>


// read/write longitude/latitude point
template <typename CoordinateType, typename CoordinateSystem>
struct rw_lon_lat_point
{
    CoordinateType longitude, latitude; 
};


namespace boost { namespace geometry { namespace traits
{
template <typename CoordinateType, typename CoordinateSystem>
struct tag<rw_lon_lat_point<CoordinateType, CoordinateSystem> >
{
    typedef point_tag type;
};

template <typename CoordinateType, typename CoordinateSystem>
struct coordinate_type<rw_lon_lat_point<CoordinateType, CoordinateSystem> >
{
    typedef CoordinateType type;
};

template <typename CoordinateType, typename CoordinateSystem>
struct coordinate_system<rw_lon_lat_point<CoordinateType, CoordinateSystem> >
{
    typedef CoordinateSystem type;
};

template <typename CoordinateType, typename CoordinateSystem>
struct dimension<rw_lon_lat_point<CoordinateType, CoordinateSystem> >
    : boost::mpl::int_<2>
{};

template
<
    typename CoordinateType,
    typename CoordinateSystem,
    std::size_t Dimension
>
struct access<rw_lon_lat_point<CoordinateType, CoordinateSystem>, Dimension>
{
    static inline CoordinateType
    get(rw_lon_lat_point<CoordinateType, CoordinateSystem> const& p)
    {
        return (Dimension == 0) ? p.longitude : p.latitude;
    }

    static inline
    void set(rw_lon_lat_point<CoordinateType, CoordinateSystem>& p,
             CoordinateType const& value)
    {
        ( Dimension == 0 ? p.longitude : p.latitude ) = value;
    }
};

}}} // namespace boost::geometry::traits


// read-only longitude/latitude point
template <typename CoordinateType, typename CoordinateSystem>
struct ro_lon_lat_point
{
    CoordinateType longitude, latitude; 
};


namespace boost { namespace geometry { namespace traits
{
template <typename CoordinateType, typename CoordinateSystem>
struct tag<ro_lon_lat_point<CoordinateType, CoordinateSystem> >
{
    typedef point_tag type;
};

template <typename CoordinateType, typename CoordinateSystem>
struct coordinate_type<ro_lon_lat_point<CoordinateType, CoordinateSystem> >
{
    typedef CoordinateType type;
};

template <typename CoordinateType, typename CoordinateSystem>
struct coordinate_system<ro_lon_lat_point<CoordinateType, CoordinateSystem> >
{
    typedef CoordinateSystem type;
};

template <typename CoordinateType, typename CoordinateSystem>
struct dimension<ro_lon_lat_point<CoordinateType, CoordinateSystem> >
    : boost::mpl::int_<2>
{};

template
<
    typename CoordinateType,
    typename CoordinateSystem,
    std::size_t Dimension
>
struct access<ro_lon_lat_point<CoordinateType, CoordinateSystem>, Dimension>
{
    static inline CoordinateType
    get(ro_lon_lat_point<CoordinateType, CoordinateSystem> const& p)
    {
        return (Dimension == 0) ? p.longitude : p.latitude;
    }
};

}}} // namespace boost::geometry::traits


#endif // BOOST_GEOMETRY_TEST_TEST_GEOMETRIES_CUSTOM_LON_LAT_POINT_HPP
