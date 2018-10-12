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

#ifndef BOOST_GEOMETRY_PROJECTIONS_PUTP6_HPP
#define BOOST_GEOMETRY_PROJECTIONS_PUTP6_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/aasincos.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct putp6 {}; // Putnins P6
    struct putp6p {}; // Putnins P6'

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace putp6
    {

            static const double epsilon = 1e-10;
            static const int n_iter = 10;
            static const double CON_POLE = 1.732050807568877;

            template <typename T>
            struct par_putp6
            {
                T C_x, C_y, A, B, D;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_putp6_spheroid
                : public base_t_fi<base_putp6_spheroid<T, Parameters>, T, Parameters>
            {
                par_putp6<T> m_proj_parm;

                inline base_putp6_spheroid(const Parameters& par)
                    : base_t_fi<base_putp6_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T p, r, V;
                    int i;

                    p = this->m_proj_parm.B * sin(lp_lat);
                    lp_lat *=  1.10265779;
                    for (i = n_iter; i ; --i) {
                        r = sqrt(1. + lp_lat * lp_lat);
                        lp_lat -= V = ( (this->m_proj_parm.A - r) * lp_lat - log(lp_lat + r) - p ) /
                            (this->m_proj_parm.A - 2. * r);
                        if (fabs(V) < epsilon)
                            break;
                    }
                    if (!i)
                        lp_lat = p < 0. ? -CON_POLE : CON_POLE;
                    xy_x = this->m_proj_parm.C_x * lp_lon * (this->m_proj_parm.D - sqrt(1. + lp_lat * lp_lat));
                    xy_y = this->m_proj_parm.C_y * lp_lat;
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    T r;

                    lp_lat = xy_y / this->m_proj_parm.C_y;
                    r = sqrt(1. + lp_lat * lp_lat);
                    lp_lon = xy_x / (this->m_proj_parm.C_x * (this->m_proj_parm.D - r));
                    lp_lat = aasin(( (this->m_proj_parm.A - r) * lp_lat - log(lp_lat + r) ) / this->m_proj_parm.B);
                }

                static inline std::string get_name()
                {
                    return "putp6_spheroid";
                }

            };
            

            // Putnins P6
            template <typename Parameters, typename T>
            inline void setup_putp6(Parameters& par, par_putp6<T>& proj_parm)
            {
                proj_parm.C_x = 1.01346;
                proj_parm.C_y = 0.91910;
                proj_parm.A   = 4.;
                proj_parm.B   = 2.1471437182129378784;
                proj_parm.D   = 2.;
                
                par.es = 0.;
            }

            // Putnins P6'
            template <typename Parameters, typename T>
            inline void setup_putp6p(Parameters& par, par_putp6<T>& proj_parm)
            {
                proj_parm.C_x = 0.44329;
                proj_parm.C_y = 0.80404;
                proj_parm.A   = 6.;
                proj_parm.B   = 5.61125;
                proj_parm.D   = 3.;
                
                par.es = 0.;
            }

    }} // namespace detail::putp6
    #endif // doxygen

    /*!
        \brief Putnins P6 projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_putp6.gif
    */
    template <typename T, typename Parameters>
    struct putp6_spheroid : public detail::putp6::base_putp6_spheroid<T, Parameters>
    {
        inline putp6_spheroid(const Parameters& par) : detail::putp6::base_putp6_spheroid<T, Parameters>(par)
        {
            detail::putp6::setup_putp6(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Putnins P6' projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_putp6p.gif
    */
    template <typename T, typename Parameters>
    struct putp6p_spheroid : public detail::putp6::base_putp6_spheroid<T, Parameters>
    {
        inline putp6p_spheroid(const Parameters& par) : detail::putp6::base_putp6_spheroid<T, Parameters>(par)
        {
            detail::putp6::setup_putp6p(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::putp6, putp6_spheroid, putp6_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::putp6p, putp6p_spheroid, putp6p_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class putp6_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<putp6_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class putp6p_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<putp6p_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void putp6_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("putp6", new putp6_entry<T, Parameters>);
            factory.add_to_factory("putp6p", new putp6p_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_PUTP6_HPP

