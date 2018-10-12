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

#ifndef BOOST_GEOMETRY_PROJECTIONS_LCC_HPP
#define BOOST_GEOMETRY_PROJECTIONS_LCC_HPP

#include <boost/geometry/util/math.hpp>
#include <boost/math/special_functions/hypot.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/pj_msfn.hpp>
#include <boost/geometry/srs/projections/impl/pj_phi2.hpp>
#include <boost/geometry/srs/projections/impl/pj_tsfn.hpp>


namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct lcc {}; // Lambert Conformal Conic

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace lcc
    {
            static const double epsilon10 = 1.e-10;

            template <typename T>
            struct par_lcc
            {
                T   phi1;
                T   phi2;
                T   n;
                T   rho0;
                T   c;
                int ellips;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_lcc_ellipsoid
                : public base_t_fi<base_lcc_ellipsoid<T, Parameters>, T, Parameters>
            {
                par_lcc<T> m_proj_parm;

                inline base_lcc_ellipsoid(const Parameters& par)
                    : base_t_fi<base_lcc_ellipsoid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(e_forward)  ellipsoid & spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    static const T fourth_pi = detail::fourth_pi<T>();
                    static const T half_pi = detail::half_pi<T>();

                    T rho;

                    if (fabs(fabs(lp_lat) - half_pi) < epsilon10) {
                        if ((lp_lat * this->m_proj_parm.n) <= 0.) {
                            BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                        }
                        rho = 0.;
                    } else {
                        rho = this->m_proj_parm.c * (this->m_proj_parm.ellips
                            ? math::pow(pj_tsfn(lp_lat, sin(lp_lat), this->m_par.e), this->m_proj_parm.n)
                            : math::pow(tan(fourth_pi + T(0.5) * lp_lat), -this->m_proj_parm.n));
                    }
                    lp_lon *= this->m_proj_parm.n;
                    xy_x = this->m_par.k0 * (rho * sin( lp_lon) );
                    xy_y = this->m_par.k0 * (this->m_proj_parm.rho0 - rho * cos(lp_lon) );
                }

                // INVERSE(e_inverse)  ellipsoid & spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T half_pi = detail::half_pi<T>();

                    T rho;

                    xy_x /= this->m_par.k0;
                    xy_y /= this->m_par.k0;

                    xy_y = this->m_proj_parm.rho0 - xy_y;
                    rho = boost::math::hypot(xy_x, xy_y);
                    if(rho != 0.0) {
                        if (this->m_proj_parm.n < 0.) {
                            rho = -rho;
                            xy_x = -xy_x;
                            xy_y = -xy_y;
                        }
                        if (this->m_proj_parm.ellips) {
                            lp_lat = pj_phi2(math::pow(rho / this->m_proj_parm.c, T(1)/this->m_proj_parm.n), this->m_par.e);
                            if (lp_lat == HUGE_VAL) {
                                BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                            }
                        } else
                            lp_lat = 2. * atan(math::pow(this->m_proj_parm.c / rho, T(1)/this->m_proj_parm.n)) - half_pi;
                        lp_lon = atan2(xy_x, xy_y) / this->m_proj_parm.n;
                    } else {
                        lp_lon = 0.;
                        lp_lat = this->m_proj_parm.n > 0. ? half_pi : -half_pi;
                    }
                }

                static inline std::string get_name()
                {
                    return "lcc_ellipsoid";
                }

            };

            // Lambert Conformal Conic
            template <typename Parameters, typename T>
            inline void setup_lcc(Parameters& par, par_lcc<T>& proj_parm)
            {
                static const T fourth_pi = detail::fourth_pi<T>();
                static const T half_pi = detail::half_pi<T>();

                T cosphi, sinphi;
                int secant;

                proj_parm.phi1 = pj_get_param_r(par.params, "lat_1");
                if (pj_param_r(par.params, "lat_2", proj_parm.phi2)) {
                    /* empty */
                } else {
                    proj_parm.phi2 = proj_parm.phi1;
                    if (!pj_param_exists(par.params, "lat_0"))
                        par.phi0 = proj_parm.phi1;
                }
                if (fabs(proj_parm.phi1 + proj_parm.phi2) < epsilon10)
                    BOOST_THROW_EXCEPTION( projection_exception(error_conic_lat_equal) );

                proj_parm.n = sinphi = sin(proj_parm.phi1);
                cosphi = cos(proj_parm.phi1);
                secant = fabs(proj_parm.phi1 - proj_parm.phi2) >= epsilon10;
                if( (proj_parm.ellips = (par.es != 0.)) ) {
                    double ml1, m1;

                    par.e = sqrt(par.es);
                    m1 = pj_msfn(sinphi, cosphi, par.es);
                    ml1 = pj_tsfn(proj_parm.phi1, sinphi, par.e);
                    if (secant) { /* secant cone */
                        sinphi = sin(proj_parm.phi2);
                        proj_parm.n = log(m1 / pj_msfn(sinphi, cos(proj_parm.phi2), par.es));
                        proj_parm.n /= log(ml1 / pj_tsfn(proj_parm.phi2, sinphi, par.e));
                    }
                    proj_parm.c = (proj_parm.rho0 = m1 * math::pow(ml1, -proj_parm.n) / proj_parm.n);
                    proj_parm.rho0 *= (fabs(fabs(par.phi0) - half_pi) < epsilon10) ? T(0) :
                        math::pow(pj_tsfn(par.phi0, sin(par.phi0), par.e), proj_parm.n);
                } else {
                    if (secant)
                        proj_parm.n = log(cosphi / cos(proj_parm.phi2)) /
                           log(tan(fourth_pi + .5 * proj_parm.phi2) /
                           tan(fourth_pi + .5 * proj_parm.phi1));
                    proj_parm.c = cosphi * math::pow(tan(fourth_pi + T(0.5) * proj_parm.phi1), proj_parm.n) / proj_parm.n;
                    proj_parm.rho0 = (fabs(fabs(par.phi0) - half_pi) < epsilon10) ? 0. :
                        proj_parm.c * math::pow(tan(fourth_pi + T(0.5) * par.phi0), -proj_parm.n);
                }
            }

    }} // namespace detail::lcc
    #endif // doxygen

    /*!
        \brief Lambert Conformal Conic projection
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
         - lat_2: Latitude of second standard parallel (degrees)
         - lat_0: Latitude of origin
        \par Example
        \image html ex_lcc.gif
    */
    template <typename T, typename Parameters>
    struct lcc_ellipsoid : public detail::lcc::base_lcc_ellipsoid<T, Parameters>
    {
        inline lcc_ellipsoid(const Parameters& par) : detail::lcc::base_lcc_ellipsoid<T, Parameters>(par)
        {
            detail::lcc::setup_lcc(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::lcc, lcc_ellipsoid, lcc_ellipsoid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class lcc_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<lcc_ellipsoid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void lcc_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("lcc", new lcc_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_LCC_HPP

