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

#ifndef BOOST_GEOMETRY_PROJECTIONS_GN_SINU_HPP
#define BOOST_GEOMETRY_PROJECTIONS_GN_SINU_HPP

#include <boost/geometry/util/math.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/aasincos.hpp>
#include <boost/geometry/srs/projections/impl/pj_mlfn.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct gn_sinu {}; // General Sinusoidal Series
    struct sinu {};    // Sinusoidal (Sanson-Flamsteed)
    struct eck6 {};    // Eckert VI
    struct mbtfps {};  // McBryde-Thomas Flat-Polar Sinusoidal

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace gn_sinu
    {

            static const double epsilon10 = 1e-10;
            static const int max_iter = 8;
            static const double loop_tol = 1e-7;

            template <typename T>
            struct par_gn_sinu
            {
                detail::en<T> en;
                T    m, n, C_x, C_y;
            };

            /* Ellipsoidal Sinusoidal only */

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_gn_sinu_ellipsoid
                : public base_t_fi<base_gn_sinu_ellipsoid<T, Parameters>, T, Parameters>
            {
                par_gn_sinu<T> m_proj_parm;

                inline base_gn_sinu_ellipsoid(const Parameters& par)
                    : base_t_fi<base_gn_sinu_ellipsoid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(e_forward)  ellipsoid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T s, c;

                    xy_y = pj_mlfn(lp_lat, s = sin(lp_lat), c = cos(lp_lat), this->m_proj_parm.en);
                    xy_x = lp_lon * c / sqrt(1. - this->m_par.es * s * s);
                }

                // INVERSE(e_inverse)  ellipsoid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T half_pi = detail::half_pi<T>();

                    T s;

                    if ((s = fabs(lp_lat = pj_inv_mlfn(xy_y, this->m_par.es, this->m_proj_parm.en))) < half_pi) {
                        s = sin(lp_lat);
                        lp_lon = xy_x * sqrt(1. - this->m_par.es * s * s) / cos(lp_lat);
                    } else if ((s - epsilon10) < half_pi)
                        lp_lon = 0.;
                    else
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                }
                /* General spherical sinusoidals */

                static inline std::string get_name()
                {
                    return "gn_sinu_ellipsoid";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_gn_sinu_spheroid
                : public base_t_fi<base_gn_sinu_spheroid<T, Parameters>, T, Parameters>
            {
                par_gn_sinu<T> m_proj_parm;

                inline base_gn_sinu_spheroid(const Parameters& par)
                    : base_t_fi<base_gn_sinu_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  sphere
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    if (this->m_proj_parm.m == 0.0)
                        lp_lat = this->m_proj_parm.n != 1. ? aasin(this->m_proj_parm.n * sin(lp_lat)): lp_lat;
                    else {
                        T k, V;
                        int i;

                        k = this->m_proj_parm.n * sin(lp_lat);
                        for (i = max_iter; i ; --i) {
                            lp_lat -= V = (this->m_proj_parm.m * lp_lat + sin(lp_lat) - k) /
                                (this->m_proj_parm.m + cos(lp_lat));
                            if (fabs(V) < loop_tol)
                                break;
                        }
                        if (!i) {
                            BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                        }
                    }
                    xy_x = this->m_proj_parm.C_x * lp_lon * (this->m_proj_parm.m + cos(lp_lat));
                    xy_y = this->m_proj_parm.C_y * lp_lat;
                }

                // INVERSE(s_inverse)  sphere
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    xy_y /= this->m_proj_parm.C_y;
                    lp_lat = (this->m_proj_parm.m != 0.0) ? aasin((this->m_proj_parm.m * xy_y + sin(xy_y)) / this->m_proj_parm.n) :
                        ( this->m_proj_parm.n != 1. ? aasin(sin(xy_y) / this->m_proj_parm.n) : xy_y );
                    lp_lon = xy_x / (this->m_proj_parm.C_x * (this->m_proj_parm.m + cos(xy_y)));
                }

                static inline std::string get_name()
                {
                    return "gn_sinu_spheroid";
                }

            };

            template <typename Parameters, typename T>
            inline void setup(Parameters& par, par_gn_sinu<T>& proj_parm) 
            {
                par.es = 0;

                proj_parm.C_x = (proj_parm.C_y = sqrt((proj_parm.m + 1.) / proj_parm.n))/(proj_parm.m + 1.);
            }


            // General Sinusoidal Series
            template <typename Parameters, typename T>
            inline void setup_gn_sinu(Parameters& par, par_gn_sinu<T>& proj_parm)
            {
                if (pj_param_f(par.params, "n", proj_parm.n)
                 && pj_param_f(par.params, "m", proj_parm.m)) {
                    if (proj_parm.n <= 0 || proj_parm.m < 0)
                        BOOST_THROW_EXCEPTION( projection_exception(error_invalid_m_or_n) );
                } else
                    BOOST_THROW_EXCEPTION( projection_exception(error_invalid_m_or_n) );

                setup(par, proj_parm);
            }

            // Sinusoidal (Sanson-Flamsteed)
            template <typename Parameters, typename T>
            inline void setup_sinu(Parameters& par, par_gn_sinu<T>& proj_parm)
            {
                proj_parm.en = pj_enfn<T>(par.es);

                if (par.es != 0.0) {
                    /* empty */
                } else {
                    proj_parm.n = 1.;
                    proj_parm.m = 0.;
                    setup(par, proj_parm);
                }
            }

            // Eckert VI
            template <typename Parameters, typename T>
            inline void setup_eck6(Parameters& par, par_gn_sinu<T>& proj_parm)
            {
                proj_parm.m = 1.;
                proj_parm.n = 2.570796326794896619231321691;
                setup(par, proj_parm);
            }

            // McBryde-Thomas Flat-Polar Sinusoidal
            template <typename Parameters, typename T>
            inline void setup_mbtfps(Parameters& par, par_gn_sinu<T>& proj_parm)
            {
                proj_parm.m = 0.5;
                proj_parm.n = 1.785398163397448309615660845;
                setup(par, proj_parm);
            }

    }} // namespace detail::gn_sinu
    #endif // doxygen

    /*!
        \brief General Sinusoidal Series projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Projection parameters
         - m (real)
         - n (real)
        \par Example
        \image html ex_gn_sinu.gif
    */
    template <typename T, typename Parameters>
    struct gn_sinu_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>
    {
        inline gn_sinu_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>(par)
        {
            detail::gn_sinu::setup_gn_sinu(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Sinusoidal (Sanson-Flamsteed) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
         - Ellipsoid
        \par Example
        \image html ex_sinu.gif
    */
    template <typename T, typename Parameters>
    struct sinu_ellipsoid : public detail::gn_sinu::base_gn_sinu_ellipsoid<T, Parameters>
    {
        inline sinu_ellipsoid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_ellipsoid<T, Parameters>(par)
        {
            detail::gn_sinu::setup_sinu(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Sinusoidal (Sanson-Flamsteed) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
         - Ellipsoid
        \par Example
        \image html ex_sinu.gif
    */
    template <typename T, typename Parameters>
    struct sinu_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>
    {
        inline sinu_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>(par)
        {
            detail::gn_sinu::setup_sinu(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief Eckert VI projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_eck6.gif
    */
    template <typename T, typename Parameters>
    struct eck6_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>
    {
        inline eck6_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>(par)
        {
            detail::gn_sinu::setup_eck6(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief McBryde-Thomas Flat-Polar Sinusoidal projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_mbtfps.gif
    */
    template <typename T, typename Parameters>
    struct mbtfps_spheroid : public detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>
    {
        inline mbtfps_spheroid(const Parameters& par) : detail::gn_sinu::base_gn_sinu_spheroid<T, Parameters>(par)
        {
            detail::gn_sinu::setup_mbtfps(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::gn_sinu, gn_sinu_spheroid, gn_sinu_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::sinu, sinu_spheroid, sinu_ellipsoid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::eck6, eck6_spheroid, eck6_spheroid)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::mbtfps, mbtfps_spheroid, mbtfps_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class gn_sinu_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<gn_sinu_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class sinu_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    if (par.es)
                        return new base_v_fi<sinu_ellipsoid<T, Parameters>, T, Parameters>(par);
                    else
                        return new base_v_fi<sinu_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class eck6_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<eck6_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        class mbtfps_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<mbtfps_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void gn_sinu_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("gn_sinu", new gn_sinu_entry<T, Parameters>);
            factory.add_to_factory("sinu", new sinu_entry<T, Parameters>);
            factory.add_to_factory("eck6", new eck6_entry<T, Parameters>);
            factory.add_to_factory("mbtfps", new mbtfps_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_GN_SINU_HPP

