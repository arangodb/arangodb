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

#ifndef BOOST_GEOMETRY_PROJECTIONS_STS_HPP
#define BOOST_GEOMETRY_PROJECTIONS_STS_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/aasincos.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct kav5 {};    // Kavraisky V
    struct qua_aut {}; // Quartic Authalic
    struct fouc {};    // Foucaut
    struct mbt_s {};   // McBryde-Thomas Flat-Polar Sine (No. 1)

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace sts
    {
            template <typename T>
            struct par_sts
            {
                T C_x, C_y, C_p;
                bool tan_mode;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_sts_spheroid
                : public base_t_fi<base_sts_spheroid<T, Parameters>, T, Parameters>
            {
                par_sts<T> m_proj_parm;

                inline base_sts_spheroid(const Parameters& par)
                    : base_t_fi<base_sts_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T c;

                    xy_x = this->m_proj_parm.C_x * lp_lon * cos(lp_lat);
                    xy_y = this->m_proj_parm.C_y;
                    lp_lat *= this->m_proj_parm.C_p;
                    c = cos(lp_lat);
                    if (this->m_proj_parm.tan_mode) {
                        xy_x *= c * c;
                        xy_y *= tan(lp_lat);
                    } else {
                        xy_x /= c;
                        xy_y *= sin(lp_lat);
                    }
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    T c;

                    xy_y /= this->m_proj_parm.C_y;
                    c = cos(lp_lat = this->m_proj_parm.tan_mode ? atan(xy_y) : aasin(xy_y));
                    lp_lat /= this->m_proj_parm.C_p;
                    lp_lon = xy_x / (this->m_proj_parm.C_x * cos(lp_lat));
                    if (this->m_proj_parm.tan_mode)
                        lp_lon /= c * c;
                    else
                        lp_lon *= c;
                }

                static inline std::string get_name()
                {
                    return "sts_spheroid";
                }

            };

            template <typename Parameters, typename T>
            inline void setup(Parameters& par, par_sts<T>& proj_parm, T const& p, T const& q, bool mode) 
            {
                par.es = 0.;
                proj_parm.C_x = q / p;
                proj_parm.C_y = p;
                proj_parm.C_p = 1/ q;
                proj_parm.tan_mode = mode;
            }


            // Foucaut
            template <typename Parameters, typename T>
            inline void setup_fouc(Parameters& par, par_sts<T>& proj_parm)
            {
                setup(par, proj_parm, 2., 2., true);
            }

            // Kavraisky V
            template <typename Parameters, typename T>
            inline void setup_kav5(Parameters& par, par_sts<T>& proj_parm)
            {
                setup(par, proj_parm, 1.50488, 1.35439, false);
            }

            // Quartic Authalic
            template <typename Parameters, typename T>
            inline void setup_qua_aut(Parameters& par, par_sts<T>& proj_parm)
            {
                setup(par, proj_parm, 2., 2., false);
            }

            // McBryde-Thomas Flat-Polar Sine (No. 1)
            template <typename Parameters, typename T>
            inline void setup_mbt_s(Parameters& par, par_sts<T>& proj_parm)
            {
                setup(par, proj_parm, 1.48875, 1.36509, false);
            }

    }} // namespace detail::sts
    #endif // doxygen

    /*!
        \brief Kavraisky V projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_kav5.gif
    */
    template <typename T, typename Parameters>
    struct kav5_spheroid : public detail::sts::base_sts_spheroid<T, Parameters>
    {
        inline kav5_spheroid(const Parameters& par) : detail::sts::base_sts_spheroid<T, Parameters>(par)
        {
            detail::sts::setup_kav5(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Quartic Authalic projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_qua_aut.gif
    */
    template <typename T, typename Parameters>
    struct qua_aut_spheroid : public detail::sts::base_sts_spheroid<T, Parameters>
    {
        inline qua_aut_spheroid(const Parameters& par) : detail::sts::base_sts_spheroid<T, Parameters>(par)
        {
            detail::sts::setup_qua_aut(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief McBryde-Thomas Flat-Polar Sine (No. 1) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_mbt_s.gif
    */
    template <typename T, typename Parameters>
    struct mbt_s_spheroid : public detail::sts::base_sts_spheroid<T, Parameters>
    {
        inline mbt_s_spheroid(const Parameters& par) : detail::sts::base_sts_spheroid<T, Parameters>(par)
        {
            detail::sts::setup_mbt_s(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Foucaut projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_fouc.gif
    */
    template <typename T, typename Parameters>
    struct fouc_spheroid : public detail::sts::base_sts_spheroid<T, Parameters>
    {
        inline fouc_spheroid(const Parameters& par) : detail::sts::base_sts_spheroid<T, Parameters>(par)
        {
            detail::sts::setup_fouc(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::kav5, kav5_spheroid, kav5_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::qua_aut, qua_aut_spheroid, qua_aut_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::mbt_s, mbt_s_spheroid, mbt_s_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::fouc, fouc_spheroid, fouc_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class kav5_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<kav5_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class qua_aut_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<qua_aut_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class mbt_s_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<mbt_s_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class fouc_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<fouc_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void sts_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("kav5", new kav5_entry<T, Parameters>);
            factory.add_to_factory("qua_aut", new qua_aut_entry<T, Parameters>);
            factory.add_to_factory("mbt_s", new mbt_s_entry<T, Parameters>);
            factory.add_to_factory("fouc", new fouc_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_STS_HPP

