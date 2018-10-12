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

#ifndef BOOST_GEOMETRY_PROJECTIONS_PUTP3_HPP
#define BOOST_GEOMETRY_PROJECTIONS_PUTP3_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct putp3 {}; // Putnins P3
    struct putp3p {}; // Putnins P3'

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace putp3
    {

            static const double C = 0.79788456;
            static const double RPISQ = 0.1013211836;

            template <typename T>
            struct par_putp3
            {
                T    A;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_putp3_spheroid
                : public base_t_fi<base_putp3_spheroid<T, Parameters>, T, Parameters>
            {
                par_putp3<T> m_proj_parm;

                inline base_putp3_spheroid(const Parameters& par)
                    : base_t_fi<base_putp3_spheroid<T, Parameters>,
                     T, Parameters>(*this, par) {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    xy_x = C * lp_lon * (1. - this->m_proj_parm.A * lp_lat * lp_lat);
                    xy_y = C * lp_lat;
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    lp_lat = xy_y / C;
                    lp_lon = xy_x / (C * (1. - this->m_proj_parm.A * lp_lat * lp_lat));
                }

                static inline std::string get_name()
                {
                    return "putp3_spheroid";
                }

            };


            // Putnins P3
            template <typename Parameters, typename T>
            inline void setup_putp3(Parameters& par, par_putp3<T>& proj_parm)
            {
                proj_parm.A = 4. * RPISQ;
                
                par.es = 0.;
            }

            // Putnins P3'
            template <typename Parameters, typename T>
            inline void setup_putp3p(Parameters& par, par_putp3<T>& proj_parm)
            {
                proj_parm.A = 2. * RPISQ;
                
                par.es = 0.;
            }

    }} // namespace detail::putp3
    #endif // doxygen

    /*!
        \brief Putnins P3 projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_putp3.gif
    */
    template <typename T, typename Parameters>
    struct putp3_spheroid : public detail::putp3::base_putp3_spheroid<T, Parameters>
    {
        inline putp3_spheroid(const Parameters& par) : detail::putp3::base_putp3_spheroid<T, Parameters>(par)
        {
            detail::putp3::setup_putp3(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Putnins P3' projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_putp3p.gif
    */
    template <typename T, typename Parameters>
    struct putp3p_spheroid : public detail::putp3::base_putp3_spheroid<T, Parameters>
    {
        inline putp3p_spheroid(const Parameters& par) : detail::putp3::base_putp3_spheroid<T, Parameters>(par)
        {
            detail::putp3::setup_putp3p(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::putp3, putp3_spheroid, putp3_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::putp3p, putp3p_spheroid, putp3p_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class putp3_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<putp3_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class putp3p_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<putp3p_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void putp3_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("putp3", new putp3_entry<T, Parameters>);
            factory.add_to_factory("putp3p", new putp3p_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_PUTP3_HPP

