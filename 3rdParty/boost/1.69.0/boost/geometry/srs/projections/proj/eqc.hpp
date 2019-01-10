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

#ifndef BOOST_GEOMETRY_PROJECTIONS_EQC_HPP
#define BOOST_GEOMETRY_PROJECTIONS_EQC_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace eqc
    {
            template <typename T>
            struct par_eqc
            {
                T rc;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_eqc_spheroid
                : public base_t_fi<base_eqc_spheroid<T, Parameters>, T, Parameters>
            {
                par_eqc<T> m_proj_parm;

                inline base_eqc_spheroid(const Parameters& par)
                    : base_t_fi<base_eqc_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T const& lp_lon, T const& lp_lat, T& xy_x, T& xy_y) const
                {
                    xy_x = this->m_proj_parm.rc * lp_lon;
                    xy_y = lp_lat - this->m_par.phi0;
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T const& xy_x, T const& xy_y, T& lp_lon, T& lp_lat) const
                {
                    lp_lon = xy_x / this->m_proj_parm.rc;
                    lp_lat = xy_y + this->m_par.phi0;
                }

                static inline std::string get_name()
                {
                    return "eqc_spheroid";
                }

            };

            // Equidistant Cylindrical (Plate Caree)
            template <typename Params, typename Parameters, typename T>
            inline void setup_eqc(Params const& params, Parameters& par, par_eqc<T>& proj_parm)
            {
                proj_parm.rc = cos(pj_get_param_r<T, srs::spar::lat_ts>(params, "lat_ts", srs::dpar::lat_ts));
                if (proj_parm.rc <= 0.)
                    BOOST_THROW_EXCEPTION( projection_exception(error_lat_ts_larger_than_90) );
                par.es = 0.;
            }

    }} // namespace detail::eqc
    #endif // doxygen

    /*!
        \brief Equidistant Cylindrical (Plate Caree) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Cylindrical
         - Spheroid
        \par Projection parameters
         - lat_ts: Latitude of true scale (degrees)
         - lat_0: Latitude of origin
        \par Example
        \image html ex_eqc.gif
    */
    template <typename T, typename Parameters>
    struct eqc_spheroid : public detail::eqc::base_eqc_spheroid<T, Parameters>
    {
        template <typename Params>
        inline eqc_spheroid(Params const& params, Parameters const& par)
            : detail::eqc::base_eqc_spheroid<T, Parameters>(par)
        {
            detail::eqc::setup_eqc(params, this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::spar::proj_eqc, eqc_spheroid, eqc_spheroid)

        // Factory entry(s)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_ENTRY_FI(eqc_entry, eqc_spheroid)
        
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_INIT_BEGIN(eqc_init)
        {
            BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_INIT_ENTRY(eqc, eqc_entry);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_EQC_HPP

