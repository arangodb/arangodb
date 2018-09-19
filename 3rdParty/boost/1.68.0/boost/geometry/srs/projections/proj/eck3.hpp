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

#ifndef BOOST_GEOMETRY_PROJECTIONS_ECK3_HPP
#define BOOST_GEOMETRY_PROJECTIONS_ECK3_HPP

#include <boost/core/ignore_unused.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/aasincos.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct eck3 {};  // Eckert III
    struct putp1 {}; // Putnins P1
    struct wag6 {};  // Wagner VI
    struct kav7 {};  // Kavraisky VII

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace eck3
    {

            template <typename T>
            struct par_eck3
            {
                T C_x, C_y, A, B;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_eck3_spheroid
                : public base_t_fi<base_eck3_spheroid<T, Parameters>, T, Parameters>
            {
                par_eck3<T> m_proj_parm;

                inline base_eck3_spheroid(const Parameters& par)
                    : base_t_fi<base_eck3_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    xy_y = this->m_proj_parm.C_y * lp_lat;
                    xy_x = this->m_proj_parm.C_x * lp_lon * (this->m_proj_parm.A + asqrt(1. - this->m_proj_parm.B * lp_lat * lp_lat));
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    T denominator;
                    lp_lat = xy_y / this->m_proj_parm.C_y;
                    denominator = (this->m_proj_parm.C_x * (this->m_proj_parm.A + asqrt(1. - this->m_proj_parm.B * lp_lat * lp_lat)));
                    if ( denominator == 0.0) {
                        lp_lon = HUGE_VAL;
                        lp_lat = HUGE_VAL;
                    } else
                        lp_lon = xy_x / denominator;
                }

                static inline std::string get_name()
                {
                    return "eck3_spheroid";
                }

            };

            template <typename Parameters>
            inline void setup(Parameters& par)
            {
                par.es = 0.;
            }


            // Eckert III
            template <typename Parameters, typename T>
            inline void setup_eck3(Parameters& par, par_eck3<T>& proj_parm)
            {
                proj_parm.C_x = 0.42223820031577120149;
                proj_parm.C_y = 0.84447640063154240298;
                proj_parm.A = 1.0;
                proj_parm.B = 0.4052847345693510857755;

                setup(par);
            }

            // Putnins P1
            template <typename Parameters, typename T>
            inline void setup_putp1(Parameters& par, par_eck3<T>& proj_parm)
            {
                proj_parm.C_x = 1.89490;
                proj_parm.C_y = 0.94745;
                proj_parm.A = -0.5;
                proj_parm.B = 0.30396355092701331433;

                setup(par);
            }

            // Wagner VI
            template <typename Parameters, typename T>
            inline void setup_wag6(Parameters& par, par_eck3<T>& proj_parm)
            {
                proj_parm.C_x = proj_parm.C_y = 0.94745;
                proj_parm.A = 0.0;
                proj_parm.B = 0.30396355092701331433;

                setup(par);
            }

            // Kavraisky VII
            template <typename Parameters, typename T>
            inline void setup_kav7(Parameters& par, par_eck3<T>& proj_parm)
            {
                /* Defined twice in original code - Using 0.866...,
                 * but leaving the other one here as a safety measure.
                 * proj_parm.C_x = 0.2632401569273184856851; */
                proj_parm.C_x = 0.8660254037844;
                proj_parm.C_y = 1.0;
                proj_parm.A = 0.0;
                proj_parm.B = 0.30396355092701331433;

                setup(par);
            }

    }} // namespace detail::eck3
    #endif // doxygen

    /*!
        \brief Eckert III projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_eck3.gif
    */
    template <typename T, typename Parameters>
    struct eck3_spheroid : public detail::eck3::base_eck3_spheroid<T, Parameters>
    {
        inline eck3_spheroid(const Parameters& par) : detail::eck3::base_eck3_spheroid<T, Parameters>(par)
        {
            detail::eck3::setup_eck3(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Putnins P1 projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_putp1.gif
    */
    template <typename T, typename Parameters>
    struct putp1_spheroid : public detail::eck3::base_eck3_spheroid<T, Parameters>
    {
        inline putp1_spheroid(const Parameters& par) : detail::eck3::base_eck3_spheroid<T, Parameters>(par)
        {
            detail::eck3::setup_putp1(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Wagner VI projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_wag6.gif
    */
    template <typename T, typename Parameters>
    struct wag6_spheroid : public detail::eck3::base_eck3_spheroid<T, Parameters>
    {
        inline wag6_spheroid(const Parameters& par) : detail::eck3::base_eck3_spheroid<T, Parameters>(par)
        {
            detail::eck3::setup_wag6(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Kavraisky VII projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_kav7.gif
    */
    template <typename T, typename Parameters>
    struct kav7_spheroid : public detail::eck3::base_eck3_spheroid<T, Parameters>
    {
        inline kav7_spheroid(const Parameters& par) : detail::eck3::base_eck3_spheroid<T, Parameters>(par)
        {
            detail::eck3::setup_kav7(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::eck3, eck3_spheroid, eck3_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::putp1, putp1_spheroid, putp1_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::wag6, wag6_spheroid, wag6_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::kav7, kav7_spheroid, kav7_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class eck3_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<eck3_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class putp1_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<putp1_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class wag6_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<wag6_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class kav7_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<kav7_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void eck3_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("eck3", new eck3_entry<T, Parameters>);
            factory.add_to_factory("putp1", new putp1_entry<T, Parameters>);
            factory.add_to_factory("wag6", new wag6_entry<T, Parameters>);
            factory.add_to_factory("kav7", new kav7_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_ECK3_HPP

