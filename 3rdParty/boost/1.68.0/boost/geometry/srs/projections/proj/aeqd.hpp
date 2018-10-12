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

// Purpose:  Implementation of the aeqd (Azimuthal Equidistant) projection.
// Author:   Gerald Evenden
// Copyright (c) 1995, Gerald Evenden

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

#ifndef BOOST_GEOMETRY_PROJECTIONS_AEQD_HPP
#define BOOST_GEOMETRY_PROJECTIONS_AEQD_HPP

#include <boost/config.hpp>
#include <boost/geometry/formulas/vincenty_direct.hpp>
#include <boost/geometry/formulas/vincenty_inverse.hpp>
#include <boost/geometry/util/math.hpp>
#include <boost/math/special_functions/hypot.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/aasincos.hpp>
#include <boost/geometry/srs/projections/impl/pj_mlfn.hpp>

#include <boost/geometry/srs/projections/par4.hpp>

#include <boost/type_traits/is_same.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct aeqd {};
    //struct aeqd_guam {};

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace aeqd
    {

            static const double epsilon10 = 1.e-10;
            static const double tolerance = 1.e-14;
            enum mode_type {
                n_pole = 0,
                s_pole = 1,
                equit  = 2,
                obliq  = 3
            };

            template <typename T>
            struct par_aeqd
            {
                T    sinph0;
                T    cosph0;
                detail::en<T> en;
                T    M1;
                T    N1;
                T    Mp;
                T    He;
                T    G;
                mode_type mode;
                srs::spheroid<T> spheroid;
            };

            template <typename T, typename Par, typename ProjParm>
            inline void e_forward(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y, Par const& par, ProjParm const& proj_parm)
            {
                T coslam, cosphi, sinphi, rho;
                //T azi1, s12;
                //T lam1, phi1, lam2, phi2;

                coslam = cos(lp_lon);
                cosphi = cos(lp_lat);
                sinphi = sin(lp_lat);
                switch (proj_parm.mode) {
                case n_pole:
                    coslam = - coslam;
                    BOOST_FALLTHROUGH;
                case s_pole:
                    xy_x = (rho = fabs(proj_parm.Mp - pj_mlfn(lp_lat, sinphi, cosphi, proj_parm.en))) *
                        sin(lp_lon);
                    xy_y = rho * coslam;
                    break;
                case equit:
                case obliq:
                    if (fabs(lp_lon) < epsilon10 && fabs(lp_lat - par.phi0) < epsilon10) {
                        xy_x = xy_y = 0.;
                        break;
                    }

                    //phi1 = par.phi0; lam1 = par.lam0;
                    //phi2 = lp_lat;  lam2 = lp_lon + par.lam0;

                    formula::result_inverse<T> const inv =
                        formula::vincenty_inverse
                            <
                                T, true, true
                            >::apply(par.lam0, par.phi0, lp_lon + par.lam0, lp_lat, proj_parm.spheroid);
                    //azi1 = inv.azimuth; s12 = inv.distance;
                    xy_x = inv.distance * sin(inv.azimuth) / par.a;
                    xy_y = inv.distance * cos(inv.azimuth) / par.a;
                    break;
                }
            }

            template <typename T, typename Par, typename ProjParm>
            inline void e_inverse(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat, Par const& par, ProjParm const& proj_parm)
            {
                T c;

                if ((c = boost::math::hypot(xy_x, xy_y)) < epsilon10) {
                    lp_lat = par.phi0;
                    lp_lon = 0.;
                        return;
                }
                if (proj_parm.mode == obliq || proj_parm.mode == equit) {
                    T const x2 = xy_x * par.a;
                    T const y2 = xy_y * par.a;
                    //T const lat1 = par.phi0;
                    //T const lon1 = par.lam0;
                    T const azi1 = atan2(x2, y2);
                    T const s12 = sqrt(x2 * x2 + y2 * y2);
                    formula::result_direct<T> const dir =
                        formula::vincenty_direct
                            <
                                T, true
                            >::apply(par.lam0, par.phi0, s12, azi1, proj_parm.spheroid);
                    lp_lat = dir.lat2;
                    lp_lon = dir.lon2;
                    lp_lon -= par.lam0;
                } else { /* Polar */
                    lp_lat = pj_inv_mlfn(proj_parm.mode == n_pole ? proj_parm.Mp - c : proj_parm.Mp + c,
                        par.es, proj_parm.en);
                    lp_lon = atan2(xy_x, proj_parm.mode == n_pole ? -xy_y : xy_y);
                }
            }

            template <typename T, typename Par, typename ProjParm>
            inline void e_guam_fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y, Par const& par, ProjParm const& proj_parm)
            {
                T cosphi, sinphi, t;

                cosphi = cos(lp_lat);
                sinphi = sin(lp_lat);
                t = 1. / sqrt(1. - par.es * sinphi * sinphi);
                xy_x = lp_lon * cosphi * t;
                xy_y = pj_mlfn(lp_lat, sinphi, cosphi, proj_parm.en) - proj_parm.M1 +
                    .5 * lp_lon * lp_lon * cosphi * sinphi * t;
            }

            template <typename T, typename Par, typename ProjParm>
            inline void e_guam_inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat, Par const& par, ProjParm const& proj_parm)
            {
                T x2, t = 0.0;
                int i;

                x2 = 0.5 * xy_x * xy_x;
                lp_lat = par.phi0;
                for (i = 0; i < 3; ++i) {
                    t = par.e * sin(lp_lat);
                    lp_lat = pj_inv_mlfn(proj_parm.M1 + xy_y -
                        x2 * tan(lp_lat) * (t = sqrt(1. - t * t)), par.es, proj_parm.en);
                }
                lp_lon = xy_x * t / cos(lp_lat);
            }

            template <typename T, typename Par, typename ProjParm>
            inline void s_forward(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y, Par const& /*par*/, ProjParm const& proj_parm)
            {
                static const T half_pi = detail::half_pi<T>();
                    
                T coslam, cosphi, sinphi;

                sinphi = sin(lp_lat);
                cosphi = cos(lp_lat);
                coslam = cos(lp_lon);
                switch (proj_parm.mode) {
                case equit:
                    xy_y = cosphi * coslam;
                    goto oblcon;
                case obliq:
                    xy_y = proj_parm.sinph0 * sinphi + proj_parm.cosph0 * cosphi * coslam;
            oblcon:
                    if (fabs(fabs(xy_y) - 1.) < tolerance)
                        if (xy_y < 0.)
                            BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                        else
                            xy_x = xy_y = 0.;
                    else {
                        xy_y = acos(xy_y);
                        xy_y /= sin(xy_y);
                        xy_x = xy_y * cosphi * sin(lp_lon);
                        xy_y *= (proj_parm.mode == equit) ? sinphi :
                                proj_parm.cosph0 * sinphi - proj_parm.sinph0 * cosphi * coslam;
                    }
                    break;
                case n_pole:
                    lp_lat = -lp_lat;
                    coslam = -coslam;
                    BOOST_FALLTHROUGH;
                case s_pole:
                    if (fabs(lp_lat - half_pi) < epsilon10)
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                    xy_x = (xy_y = (half_pi + lp_lat)) * sin(lp_lon);
                    xy_y *= coslam;
                    break;
                }
            }

            template <typename T, typename Par, typename ProjParm>
            inline void s_inverse(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat, Par const& par, ProjParm const& proj_parm)
            {
                static const T pi = detail::pi<T>();
                static const T half_pi = detail::half_pi<T>();
                    
                T cosc, c_rh, sinc;

                if ((c_rh = boost::math::hypot(xy_x, xy_y)) > pi) {
                    if (c_rh - epsilon10 > pi)
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                    c_rh = pi;
                } else if (c_rh < epsilon10) {
                    lp_lat = par.phi0;
                    lp_lon = 0.;
                        return;
                }
                if (proj_parm.mode == obliq || proj_parm.mode == equit) {
                    sinc = sin(c_rh);
                    cosc = cos(c_rh);
                    if (proj_parm.mode == equit) {
                                    lp_lat = aasin(xy_y * sinc / c_rh);
                        xy_x *= sinc;
                        xy_y = cosc * c_rh;
                    } else {
                        lp_lat = aasin(cosc * proj_parm.sinph0 + xy_y * sinc * proj_parm.cosph0 /
                            c_rh);
                        xy_y = (cosc - proj_parm.sinph0 * sin(lp_lat)) * c_rh;
                        xy_x *= sinc * proj_parm.cosph0;
                    }
                    lp_lon = xy_y == 0. ? 0. : atan2(xy_x, xy_y);
                } else if (proj_parm.mode == n_pole) {
                    lp_lat = half_pi - c_rh;
                    lp_lon = atan2(xy_x, -xy_y);
                } else {
                    lp_lat = c_rh - half_pi;
                    lp_lon = atan2(xy_x, xy_y);
                }
            }

            // Azimuthal Equidistant
            template <typename Parameters, typename T>
            inline void setup_aeqd(Parameters& par, par_aeqd<T>& proj_parm, bool is_sphere, bool is_guam)
            {
                static const T half_pi = detail::half_pi<T>();

                par.phi0 = pj_get_param_r(par.params, "lat_0");
                if (fabs(fabs(par.phi0) - half_pi) < epsilon10) {
                    proj_parm.mode = par.phi0 < 0. ? s_pole : n_pole;
                    proj_parm.sinph0 = par.phi0 < 0. ? -1. : 1.;
                    proj_parm.cosph0 = 0.;
                } else if (fabs(par.phi0) < epsilon10) {
                    proj_parm.mode = equit;
                    proj_parm.sinph0 = 0.;
                    proj_parm.cosph0 = 1.;
                } else {
                    proj_parm.mode = obliq;
                    proj_parm.sinph0 = sin(par.phi0);
                    proj_parm.cosph0 = cos(par.phi0);
                }
                if (is_sphere) {
                    /* empty */
                } else {
                    proj_parm.en = pj_enfn<T>(par.es);
                    if (is_guam) {
                        proj_parm.M1 = pj_mlfn(par.phi0, proj_parm.sinph0, proj_parm.cosph0, proj_parm.en);
                    } else {
                        switch (proj_parm.mode) {
                        case n_pole:
                            proj_parm.Mp = pj_mlfn<T>(half_pi, 1., 0., proj_parm.en);
                            break;
                        case s_pole:
                            proj_parm.Mp = pj_mlfn<T>(-half_pi, -1., 0., proj_parm.en);
                            break;
                        case equit:
                        case obliq:
                            proj_parm.N1 = 1. / sqrt(1. - par.es * proj_parm.sinph0 * proj_parm.sinph0);
                            proj_parm.G = proj_parm.sinph0 * (proj_parm.He = par.e / sqrt(par.one_es));
                            proj_parm.He *= proj_parm.cosph0;
                            break;
                        }
                        // Boost.Geometry specific, in proj4 geodesic is initialized at the beginning
                        T const b = math::sqrt(math::sqr(par.a) * (1. - par.es));
                        proj_parm.spheroid = srs::spheroid<T>(par.a, b);
                    }
                }
            }

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_aeqd_e
                : public base_t_fi<base_aeqd_e<T, Parameters>, T, Parameters>
            {
                par_aeqd<T> m_proj_parm;

                inline base_aeqd_e(const Parameters& par)
                    : base_t_fi<base_aeqd_e<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(e_forward)  elliptical
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    e_forward(lp_lon, lp_lat, xy_x, xy_y, this->m_par, this->m_proj_parm);
                }

                // INVERSE(e_inverse)  elliptical
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    e_inverse(xy_x, xy_y, lp_lon, lp_lat, this->m_par, this->m_proj_parm);
                }

                static inline std::string get_name()
                {
                    return "aeqd_e";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_aeqd_e_guam
                : public base_t_fi<base_aeqd_e_guam<T, Parameters>, T, Parameters>
            {
                par_aeqd<T> m_proj_parm;

                inline base_aeqd_e_guam(const Parameters& par)
                    : base_t_fi<base_aeqd_e_guam<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(e_guam_fwd)  Guam elliptical
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    e_guam_fwd(lp_lon, lp_lat, xy_x, xy_y, this->m_par, this->m_proj_parm);
                }

                // INVERSE(e_guam_inv)  Guam elliptical
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    e_guam_inv(xy_x, xy_y, lp_lon, lp_lat, this->m_par, this->m_proj_parm);
                }

                static inline std::string get_name()
                {
                    return "aeqd_e_guam";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename BGParameters, typename T, typename Parameters>
            struct base_aeqd_e_static
                : public base_t_fi<base_aeqd_e_static<BGParameters, T, Parameters>, T, Parameters>
            {
                par_aeqd<T> m_proj_parm;

                static const bool is_guam = ! boost::is_same
                    <
                        typename srs::par4::detail::tuples_find_if
                            <
                                BGParameters,
                                //srs::par4::detail::is_guam
                                srs::par4::detail::is_param<srs::par4::guam>::pred
                            >::type,
                        void
                    >::value;

                inline base_aeqd_e_static(const Parameters& par)
                    : base_t_fi<base_aeqd_e_static<BGParameters, T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(e_forward or e_guam_fwd)  elliptical
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    if (is_guam)
                        e_guam_fwd(lp_lon, lp_lat, xy_x, xy_y, this->m_par, this->m_proj_parm);
                    else
                        e_forward(lp_lon, lp_lat, xy_x, xy_y, this->m_par, this->m_proj_parm);
                }

                // INVERSE(e_inverse or e_guam_inv)  elliptical
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    if (is_guam)
                        e_guam_inv(xy_x, xy_y, lp_lon, lp_lat, this->m_par, this->m_proj_parm);
                    else
                        e_inverse(xy_x, xy_y, lp_lon, lp_lat, this->m_par, this->m_proj_parm);
                }

                static inline std::string get_name()
                {
                    return "aeqd_e_static";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_aeqd_s
                : public base_t_fi<base_aeqd_s<T, Parameters>, T, Parameters>
            {
                par_aeqd<T> m_proj_parm;

                inline base_aeqd_s(const Parameters& par)
                    : base_t_fi<base_aeqd_s<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spherical
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    s_forward(lp_lon, lp_lat, xy_x, xy_y, this->m_par, this->m_proj_parm);
                }

                // INVERSE(s_inverse)  spherical
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    s_inverse(xy_x, xy_y, lp_lon, lp_lat, this->m_par, this->m_proj_parm);
                }

                static inline std::string get_name()
                {
                    return "aeqd_s";
                }

            };

    }} // namespace detail::aeqd
    #endif // doxygen

    /*!
        \brief Azimuthal Equidistant projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Azimuthal
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_0: Latitude of origin (degrees)
         - guam (boolean)
        \par Example
        \image html ex_aeqd.gif
    */
    template <typename T, typename Parameters>
    struct aeqd_e : public detail::aeqd::base_aeqd_e<T, Parameters>
    {
        inline aeqd_e(const Parameters& par) : detail::aeqd::base_aeqd_e<T, Parameters>(par)
        {
            detail::aeqd::setup_aeqd(this->m_par, this->m_proj_parm, false, false);
        }
    };

    /*!
        \brief Azimuthal Equidistant projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Azimuthal
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_0: Latitude of origin (degrees)
         - guam (boolean)
        \par Example
        \image html ex_aeqd.gif
    */
    template <typename T, typename Parameters>
    struct aeqd_e_guam : public detail::aeqd::base_aeqd_e_guam<T, Parameters>
    {
        inline aeqd_e_guam(const Parameters& par) : detail::aeqd::base_aeqd_e_guam<T, Parameters>(par)
        {
            detail::aeqd::setup_aeqd(this->m_par, this->m_proj_parm, false, true);
        }
    };

    /*!
        \brief Azimuthal Equidistant projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Azimuthal
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_0: Latitude of origin (degrees)
         - guam (boolean)
        \par Example
        \image html ex_aeqd.gif
    */
    template <typename BGParameters, typename T, typename Parameters>
    struct aeqd_e_static : public detail::aeqd::base_aeqd_e_static<BGParameters, T, Parameters>
    {
        inline aeqd_e_static(const Parameters& par) : detail::aeqd::base_aeqd_e_static<BGParameters, T, Parameters>(par)
        {
            detail::aeqd::setup_aeqd(this->m_par, this->m_proj_parm,
                                     false,
                                     detail::aeqd::base_aeqd_e_static<BGParameters, T, Parameters>::is_guam);
        }
    };

    /*!
        \brief Azimuthal Equidistant projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Azimuthal
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_0: Latitude of origin (degrees)
         - guam (boolean)
        \par Example
        \image html ex_aeqd.gif
    */
    template <typename T, typename Parameters>
    struct aeqd_s : public detail::aeqd::base_aeqd_s<T, Parameters>
    {
        inline aeqd_s(const Parameters& par) : detail::aeqd::base_aeqd_s<T, Parameters>(par)
        {
            detail::aeqd::setup_aeqd(this->m_par, this->m_proj_parm, true, false);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        template <typename BGP, typename CT, typename P>
        struct static_projection_type<srs::par4::aeqd, srs_sphere_tag, BGP, CT, P>
        {
            typedef aeqd_s<CT, P> type;
        };
        template <typename BGP, typename CT, typename P>
        struct static_projection_type<srs::par4::aeqd, srs_spheroid_tag, BGP, CT, P>
        {
            typedef aeqd_e_static<BGP, CT, P> type;
        };
        //BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::aeqd, aeqd_s, aeqd_e_static)
        //BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::aeqd_guam, aeqd_guam, aeqd_guam)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class aeqd_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    bool const guam = pj_get_param_b(par.params, "guam");

                    if (par.es && ! guam)
                        return new base_v_fi<aeqd_e<T, Parameters>, T, Parameters>(par);
                    else if (par.es && guam)
                        return new base_v_fi<aeqd_e_guam<T, Parameters>, T, Parameters>(par);
                    else
                        return new base_v_fi<aeqd_s<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void aeqd_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("aeqd", new aeqd_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_AEQD_HPP

