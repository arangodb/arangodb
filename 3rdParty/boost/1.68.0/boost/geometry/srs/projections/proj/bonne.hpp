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

#ifndef BOOST_GEOMETRY_PROJECTIONS_BONNE_HPP
#define BOOST_GEOMETRY_PROJECTIONS_BONNE_HPP

#include <boost/geometry/util/math.hpp>
#include <boost/math/special_functions/hypot.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/pj_mlfn.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct bonne {};

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace bonne
    {

            static const double epsilon10 = 1e-10;

            template <typename T>
            struct par_bonne
            {
                T phi1;
                T cphi1;
                T am1;
                T m1;
                detail::en<T> en;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_bonne_ellipsoid
                : public base_t_fi<base_bonne_ellipsoid<T, Parameters>, T, Parameters>
            {
                par_bonne<T> m_proj_parm;

                inline base_bonne_ellipsoid(const Parameters& par)
                    : base_t_fi<base_bonne_ellipsoid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(e_forward)  ellipsoid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T rh, E, c;

                    rh = this->m_proj_parm.am1 + this->m_proj_parm.m1 - pj_mlfn(lp_lat, E = sin(lp_lat), c = cos(lp_lat), this->m_proj_parm.en);
                    E = c * lp_lon / (rh * sqrt(1. - this->m_par.es * E * E));
                    xy_x = rh * sin(E);
                    xy_y = this->m_proj_parm.am1 - rh * cos(E);
                }

                // INVERSE(e_inverse)  ellipsoid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T half_pi = detail::half_pi<T>();

                    T s, rh;

                    rh = boost::math::hypot(xy_x, xy_y = this->m_proj_parm.am1 - xy_y);
                    lp_lat = pj_inv_mlfn(this->m_proj_parm.am1 + this->m_proj_parm.m1 - rh, this->m_par.es, this->m_proj_parm.en);
                    if ((s = fabs(lp_lat)) < half_pi) {
                        s = sin(lp_lat);
                        lp_lon = rh * atan2(xy_x, xy_y) *
                           sqrt(1. - this->m_par.es * s * s) / cos(lp_lat);
                    } else if (fabs(s - half_pi) <= epsilon10)
                        lp_lon = 0.;
                    else
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                }

                static inline std::string get_name()
                {
                    return "bonne_ellipsoid";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_bonne_spheroid
                : public base_t_fi<base_bonne_spheroid<T, Parameters>, T, Parameters>
            {
                par_bonne<T> m_proj_parm;

                inline base_bonne_spheroid(const Parameters& par)
                    : base_t_fi<base_bonne_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T E, rh;

                    rh = this->m_proj_parm.cphi1 + this->m_proj_parm.phi1 - lp_lat;
                    if (fabs(rh) > epsilon10) {
                        xy_x = rh * sin(E = lp_lon * cos(lp_lat) / rh);
                        xy_y = this->m_proj_parm.cphi1 - rh * cos(E);
                    } else
                        xy_x = xy_y = 0.;
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T half_pi = detail::half_pi<T>();

                    T rh;

                    rh = boost::math::hypot(xy_x, xy_y = this->m_proj_parm.cphi1 - xy_y);
                    lp_lat = this->m_proj_parm.cphi1 + this->m_proj_parm.phi1 - rh;
                    if (fabs(lp_lat) > half_pi) {
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                    }
                    if (fabs(fabs(lp_lat) - half_pi) <= epsilon10)
                        lp_lon = 0.;
                    else
                        lp_lon = rh * atan2(xy_x, xy_y) / cos(lp_lat);
                }

                static inline std::string get_name()
                {
                    return "bonne_spheroid";
                }

            };

            // Bonne (Werner lat_1=90)
            template <typename Parameters, typename T>
            inline void setup_bonne(Parameters& par, par_bonne<T>& proj_parm)
            {
                static const T half_pi = detail::half_pi<T>();

                T c;

                proj_parm.phi1 = pj_get_param_r(par.params, "lat_1");
                if (fabs(proj_parm.phi1) < epsilon10)
                    BOOST_THROW_EXCEPTION( projection_exception(error_lat1_is_zero) );

                if (par.es != 0.0) {
                    proj_parm.en = pj_enfn<T>(par.es);
                    proj_parm.m1 = pj_mlfn(proj_parm.phi1, proj_parm.am1 = sin(proj_parm.phi1),
                        c = cos(proj_parm.phi1), proj_parm.en);
                    proj_parm.am1 = c / (sqrt(1. - par.es * proj_parm.am1 * proj_parm.am1) * proj_parm.am1);
                } else {
                    if (fabs(proj_parm.phi1) + epsilon10 >= half_pi)
                        proj_parm.cphi1 = 0.;
                    else
                        proj_parm.cphi1 = 1. / tan(proj_parm.phi1);
                }
            }

    }} // namespace detail::bonne
    #endif // doxygen

    /*!
        \brief Bonne (Werner lat_1=90) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Conic
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_1: Latitude of first standard parallel (degrees)
        \par Example
        \image html ex_bonne.gif
    */
    template <typename T, typename Parameters>
    struct bonne_ellipsoid : public detail::bonne::base_bonne_ellipsoid<T, Parameters>
    {
        inline bonne_ellipsoid(const Parameters& par) : detail::bonne::base_bonne_ellipsoid<T, Parameters>(par)
        {
            detail::bonne::setup_bonne(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Bonne (Werner lat_1=90) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Conic
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_1: Latitude of first standard parallel (degrees)
        \par Example
        \image html ex_bonne.gif
    */
    template <typename T, typename Parameters>
    struct bonne_spheroid : public detail::bonne::base_bonne_spheroid<T, Parameters>
    {
        inline bonne_spheroid(const Parameters& par) : detail::bonne::base_bonne_spheroid<T, Parameters>(par)
        {
            detail::bonne::setup_bonne(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::bonne, bonne_spheroid, bonne_ellipsoid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class bonne_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    if (par.es)
                        return new base_v_fi<bonne_ellipsoid<T, Parameters>, T, Parameters>(par);
                    else
                        return new base_v_fi<bonne_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void bonne_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("bonne", new bonne_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_BONNE_HPP

