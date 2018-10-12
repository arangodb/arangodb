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

#ifndef BOOST_GEOMETRY_PROJECTIONS_PUTP4P_HPP
#define BOOST_GEOMETRY_PROJECTIONS_PUTP4P_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/aasincos.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct putp4p {}; // Putnins P4'
    struct weren {}; // Werenskiold I

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace putp4p
    {
            template <typename T>
            struct par_putp4p
            {
                T    C_x, C_y;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_putp4p_spheroid
                : public base_t_fi<base_putp4p_spheroid<T, Parameters>, T, Parameters>
            {
                par_putp4p<T> m_proj_parm;

                inline base_putp4p_spheroid(const Parameters& par)
                    : base_t_fi<base_putp4p_spheroid<T, Parameters>,
                     T, Parameters>(*this, par) {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    static T const third = detail::third<T>();

                    lp_lat = aasin(0.883883476 * sin(lp_lat));
                    xy_x = this->m_proj_parm.C_x * lp_lon * cos(lp_lat);
                    xy_x /= cos(lp_lat *= third);
                    xy_y = this->m_proj_parm.C_y * sin(lp_lat);
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    lp_lat = aasin(xy_y / this->m_proj_parm.C_y);
                    lp_lon = xy_x * cos(lp_lat) / this->m_proj_parm.C_x;
                    lp_lat *= 3.;
                    lp_lon /= cos(lp_lat);
                    lp_lat = aasin(1.13137085 * sin(lp_lat));
                }

                static inline std::string get_name()
                {
                    return "putp4p_spheroid";
                }

            };


            // Putnins P4'
            template <typename Parameters, typename T>
            inline void setup_putp4p(Parameters& par, par_putp4p<T>& proj_parm)
            {
                proj_parm.C_x = 0.874038744;
                proj_parm.C_y = 3.883251825;
                
                par.es = 0.;
            }

            // Werenskiold I
            template <typename Parameters, typename T>
            inline void setup_weren(Parameters& par, par_putp4p<T>& proj_parm)
            {
                proj_parm.C_x = 1.;
                proj_parm.C_y = 4.442882938;
                
                par.es = 0.;
            }

    }} // namespace detail::putp4p
    #endif // doxygen

    /*!
        \brief Putnins P4' projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_putp4p.gif
    */
    template <typename T, typename Parameters>
    struct putp4p_spheroid : public detail::putp4p::base_putp4p_spheroid<T, Parameters>
    {
        inline putp4p_spheroid(const Parameters& par) : detail::putp4p::base_putp4p_spheroid<T, Parameters>(par)
        {
            detail::putp4p::setup_putp4p(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Werenskiold I projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_weren.gif
    */
    template <typename T, typename Parameters>
    struct weren_spheroid : public detail::putp4p::base_putp4p_spheroid<T, Parameters>
    {
        inline weren_spheroid(const Parameters& par) : detail::putp4p::base_putp4p_spheroid<T, Parameters>(par)
        {
            detail::putp4p::setup_weren(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::putp4p, putp4p_spheroid, putp4p_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::weren, weren_spheroid, weren_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class putp4p_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<putp4p_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class weren_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<weren_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void putp4p_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("putp4p", new putp4p_entry<T, Parameters>);
            factory.add_to_factory("weren", new weren_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_PUTP4P_HPP

