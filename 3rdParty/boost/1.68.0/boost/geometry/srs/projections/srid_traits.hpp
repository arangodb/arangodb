// Boost.Geometry

// Copyright (c) 2017, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_PROJECTIONS_SRID_TRAITS_HPP
#define BOOST_GEOMETRY_PROJECTIONS_SRID_TRAITS_HPP


#include <boost/geometry/srs/projections/par4.hpp>
#include <boost/geometry/srs/projections/proj4.hpp>
#include <boost/geometry/srs/sphere.hpp>
#include <boost/geometry/srs/spheroid.hpp>


#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_SRID_TRAITS_ED(AUTH, CODE, PROJ, ELLPS, DATUM, PROJ4_STR) \
template<> \
struct AUTH##_traits<CODE> \
{ \
    typedef srs::static_proj4 \
        < \
            srs::par4::proj<srs::par4::PROJ>, \
            srs::par4::ellps<srs::par4::ELLPS>, \
            srs::par4::datum<srs::par4::DATUM> \
        > static_parameters_type; \
    static inline static_parameters_type s_par() \
    { \
        return static_parameters_type(); \
    } \
    static inline srs::proj4 par() \
    { \
        return srs::proj4(PROJ4_STR); \
    } \
}; \

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_SRID_TRAITS_E(AUTH, CODE, PROJ, ELLPS, PROJ4_STR) \
template<> \
struct AUTH##_traits<CODE> \
{ \
    typedef srs::static_proj4 \
        < \
            srs::par4::proj<srs::par4::PROJ>, \
            srs::par4::ellps<srs::par4::ELLPS> \
        > static_parameters_type; \
    static inline static_parameters_type s_par() \
    { \
        return static_parameters_type(); \
    } \
    static inline srs::proj4 par() \
    { \
        return srs::proj4(PROJ4_STR); \
    } \
}; \

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_SRID_TRAITS_AB(AUTH, CODE, PROJ, A, B, PROJ4_STR) \
template<> \
struct AUTH##_traits<CODE> \
{ \
    typedef srs::static_proj4 \
        < \
            srs::par4::proj<srs::par4::PROJ>, \
            srs::par4::ellps<srs::spheroid<double> > \
        > static_parameters_type; \
    static inline static_parameters_type s_par() \
    { \
        return static_parameters_type(srs::par4::proj<srs::par4::PROJ>(), \
                                      srs::par4::ellps<srs::spheroid<double> >(srs::spheroid<double>(A, B))); \
    } \
    static inline srs::proj4 par() \
    { \
        return srs::proj4(PROJ4_STR); \
    } \
}; \

#define BOOST_GEOMETRY_PROJECTIONS_DETAIL_SRID_TRAITS_S(AUTH, CODE, PROJ, R, PROJ4_STR) \
template<> \
struct AUTH##_traits<CODE> \
{ \
    typedef srs::static_proj4 \
        < \
            srs::par4::proj<srs::par4::PROJ>, \
            srs::par4::ellps<srs::sphere<double> > \
        > static_parameters_type; \
    static inline static_parameters_type s_par() \
    { \
        return static_parameters_type(srs::par4::proj<srs::par4::PROJ>(), \
                                      srs::par4::ellps<srs::sphere<double> >(srs::sphere<double>(R))); \
    } \
    static inline srs::proj4 par() \
    { \
        return srs::proj4(PROJ4_STR); \
    } \
}; \


#endif // BOOST_GEOMETRY_PROJECTIONS_SRID_TRAITS_HPP

