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

#ifndef BOOST_GEOMETRY_PROJECTIONS_TCEA_HPP
#define BOOST_GEOMETRY_PROJECTIONS_TCEA_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace tcea
    {
            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_tcea_spheroid
                : public base_t_fi<base_tcea_spheroid<T, Parameters>, T, Parameters>
            {
                inline base_tcea_spheroid(const Parameters& par)
                    : base_t_fi<base_tcea_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T const& lp_lon, T const& lp_lat, T& xy_x, T& xy_y) const
                {
                    xy_x = cos(lp_lat) * sin(lp_lon) / this->m_par.k0;
                    xy_y = this->m_par.k0 * (atan2(tan(lp_lat), cos(lp_lon)) - this->m_par.phi0);
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T xy_x, T xy_y, T& lp_lon, T& lp_lat) const
                {
                    T t;

                    xy_y = xy_y / this->m_par.k0 + this->m_par.phi0;
                    xy_x *= this->m_par.k0;
                    t = sqrt(1. - xy_x * xy_x);
                    lp_lat = asin(t * sin(xy_y));
                    lp_lon = atan2(xy_x, t * cos(xy_y));
                }

                static inline std::string get_name()
                {
                    return "tcea_spheroid";
                }

            };

            // Transverse Cylindrical Equal Area
            template <typename Parameters>
            inline void setup_tcea(Parameters& par)
            {
                par.es = 0.;
            }

    }} // namespace detail::tcea
    #endif // doxygen
    
    /*!
        \brief Transverse Cylindrical Equal Area projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Cylindrical
         - Spheroid
        \par Example
        \image html ex_tcea.gif
    */
    template <typename T, typename Parameters>
    struct tcea_spheroid : public detail::tcea::base_tcea_spheroid<T, Parameters>
    {
        template <typename Params>
        inline tcea_spheroid(Params const& , Parameters const& par)
            : detail::tcea::base_tcea_spheroid<T, Parameters>(par)
        {
            detail::tcea::setup_tcea(this->m_par);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::spar::proj_tcea, tcea_spheroid, tcea_spheroid)

        // Factory entry(s)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_ENTRY_FI(tcea_entry, tcea_spheroid)
        
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_INIT_BEGIN(tcea_init)
        {
            BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_INIT_ENTRY(tcea, tcea_entry)
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_TCEA_HPP

