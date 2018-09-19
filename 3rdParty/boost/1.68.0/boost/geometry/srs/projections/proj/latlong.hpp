// Boost.Geometry - gis-projections (based on PROJ4)

// Copyright (c) 2008-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017, 2018.
// Modifications copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This file is converted from PROJ4, http://trac.osgeo.org/proj
// PROJ4 is originally written by Gerald Evenden (then of the USGS)
// PROJ4 is maintained by Frank Warmerdam
// PROJ4 is converted to Boost.Geometry by Barend Gehrels

// Last updated version of proj: 5.0.0

// Original copyright notice:

// Purpose:  Stub projection implementation for lat/long coordinates. We
//           don't actually change the coordinates, but we want proj=latlong
//           to act sort of like a projection.
// Author:   Frank Warmerdam, warmerdam@pobox.com
// Copyright (c) 2000, Frank Warmerdam

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef BOOST_GEOMETRY_PROJECTIONS_LATLONG_HPP
#define BOOST_GEOMETRY_PROJECTIONS_LATLONG_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>


namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct lonlat {}; // Lat/long (Geodetic)
    struct latlon {}; // Lat/long (Geodetic alias)
    struct latlong {}; // Lat/long (Geodetic alias)
    struct longlat {}; // Lat/long (Geodetic alias)

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace latlong
    {

            /* very loosely based upon DMA code by Bradford W. Drew */

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_latlong_other
                : public base_t_fi<base_latlong_other<T, Parameters>, T, Parameters>
            {
                inline base_latlong_other(const Parameters& par)
                    : base_t_fi<base_latlong_other<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(forward)
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    // TODO: in the original code a is not used
                    // different mechanism is probably used instead
                    xy_x = lp_lon / this->m_par.a;
                    xy_y = lp_lat / this->m_par.a;
                }

                // INVERSE(inverse)
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    // TODO: in the original code a is not used
                    // different mechanism is probably used instead
                    lp_lat = xy_y * this->m_par.a;
                    lp_lon = xy_x * this->m_par.a;
                }

                static inline std::string get_name()
                {
                    return "latlong_other";
                }

            };

            // Lat/long (Geodetic)
            template <typename Parameters>
            inline void setup_lonlat(Parameters& par)
            {
                    par.is_latlong = 1;
                    par.x0 = 0.0;
                    par.y0 = 0.0;
            }

            // Lat/long (Geodetic alias)
            template <typename Parameters>
            inline void setup_latlon(Parameters& par)
            {
                    par.is_latlong = 1;
                    par.x0 = 0.0;
                    par.y0 = 0.0;
            }

            // Lat/long (Geodetic alias)
            template <typename Parameters>
            inline void setup_latlong(Parameters& par)
            {
                    par.is_latlong = 1;
                    par.x0 = 0.0;
                    par.y0 = 0.0;
            }

            // Lat/long (Geodetic alias)
            template <typename Parameters>
            inline void setup_longlat(Parameters& par)
            {
                    par.is_latlong = 1;
                    par.x0 = 0.0;
                    par.y0 = 0.0;
            }

    }} // namespace detail::latlong
    #endif // doxygen

    /*!
        \brief Lat/long (Geodetic) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Example
        \image html ex_lonlat.gif
    */
    template <typename T, typename Parameters>
    struct lonlat_other : public detail::latlong::base_latlong_other<T, Parameters>
    {
        inline lonlat_other(const Parameters& par) : detail::latlong::base_latlong_other<T, Parameters>(par)
        {
            detail::latlong::setup_lonlat(this->m_par);
        }
    };

    /*!
        \brief Lat/long (Geodetic alias) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Example
        \image html ex_latlon.gif
    */
    template <typename T, typename Parameters>
    struct latlon_other : public detail::latlong::base_latlong_other<T, Parameters>
    {
        inline latlon_other(const Parameters& par) : detail::latlong::base_latlong_other<T, Parameters>(par)
        {
            detail::latlong::setup_latlon(this->m_par);
        }
    };

    /*!
        \brief Lat/long (Geodetic alias) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Example
        \image html ex_latlong.gif
    */
    template <typename T, typename Parameters>
    struct latlong_other : public detail::latlong::base_latlong_other<T, Parameters>
    {
        inline latlong_other(const Parameters& par) : detail::latlong::base_latlong_other<T, Parameters>(par)
        {
            detail::latlong::setup_latlong(this->m_par);
        }
    };

    /*!
        \brief Lat/long (Geodetic alias) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Example
        \image html ex_longlat.gif
    */
    template <typename T, typename Parameters>
    struct longlat_other : public detail::latlong::base_latlong_other<T, Parameters>
    {
        inline longlat_other(const Parameters& par) : detail::latlong::base_latlong_other<T, Parameters>(par)
        {
            detail::latlong::setup_longlat(this->m_par);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::lonlat, lonlat_other, lonlat_other)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::latlon, latlon_other, latlon_other)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::latlong, latlong_other, latlong_other)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::longlat, longlat_other, longlat_other)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class lonlat_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<lonlat_other<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class latlon_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<latlon_other<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class latlong_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<latlong_other<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class longlat_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<longlat_other<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void latlong_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("lonlat", new lonlat_entry<T, Parameters>);
            factory.add_to_factory("latlon", new latlon_entry<T, Parameters>);
            factory.add_to_factory("latlong", new latlong_entry<T, Parameters>);
            factory.add_to_factory("longlat", new longlat_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_LATLONG_HPP

