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

#ifndef BOOST_GEOMETRY_PROJECTIONS_MERC_HPP
#define BOOST_GEOMETRY_PROJECTIONS_MERC_HPP

#include <boost/geometry/util/math.hpp>

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
    struct merc {}; // Mercator

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace merc
    {

            static const double epsilon10 = 1.e-10;

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_merc_ellipsoid
                : public base_t_fi<base_merc_ellipsoid<T, Parameters>, T, Parameters>
            {
                inline base_merc_ellipsoid(const Parameters& par)
                    : base_t_fi<base_merc_ellipsoid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(e_forward)  ellipsoid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    static const T half_pi = detail::half_pi<T>();

                    if (fabs(fabs(lp_lat) - half_pi) <= epsilon10) {
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                    }
                    xy_x = this->m_par.k0 * lp_lon;
                    xy_y = - this->m_par.k0 * log(pj_tsfn(lp_lat, sin(lp_lat), this->m_par.e));
                }

                // INVERSE(e_inverse)  ellipsoid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    if ((lp_lat = pj_phi2(exp(- xy_y / this->m_par.k0), this->m_par.e)) == HUGE_VAL) {
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                    }
                    lp_lon = xy_x / this->m_par.k0;
                }

                static inline std::string get_name()
                {
                    return "merc_ellipsoid";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_merc_spheroid
                : public base_t_fi<base_merc_spheroid<T, Parameters>, T, Parameters>
            {
                inline base_merc_spheroid(const Parameters& par)
                    : base_t_fi<base_merc_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    static const T half_pi = detail::half_pi<T>();
                    static const T fourth_pi = detail::fourth_pi<T>();

                    if (fabs(fabs(lp_lat) - half_pi) <= epsilon10) {
                        BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                    }
                    xy_x = this->m_par.k0 * lp_lon;
                    xy_y = this->m_par.k0 * log(tan(fourth_pi + .5 * lp_lat));
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T half_pi = detail::half_pi<T>();

                    lp_lat = half_pi - 2. * atan(exp(-xy_y / this->m_par.k0));
                    lp_lon = xy_x / this->m_par.k0;
                }

                static inline std::string get_name()
                {
                    return "merc_spheroid";
                }

            };

            // Mercator
            template <typename Parameters>
            inline void setup_merc(Parameters& par)
            {
                typedef typename Parameters::type calc_t;
                static const calc_t half_pi = detail::half_pi<calc_t>();

                calc_t phits=0.0;
                int is_phits;

                if( (is_phits = pj_param_r(par.params, "lat_ts", phits)) ) {
                    phits = fabs(phits);
                    if (phits >= half_pi)
                        BOOST_THROW_EXCEPTION( projection_exception(error_lat_ts_larger_than_90) );
                }
                if (par.es != 0.0) { /* ellipsoid */
                    if (is_phits)
                        par.k0 = pj_msfn(sin(phits), cos(phits), par.es);
                } else { /* sphere */
                    if (is_phits)
                        par.k0 = cos(phits);
                }
            }

    }} // namespace detail::merc
    #endif // doxygen

    /*!
        \brief Mercator projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Cylindrical
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_ts: Latitude of true scale (degrees)
        \par Example
        \image html ex_merc.gif
    */
    template <typename T, typename Parameters>
    struct merc_ellipsoid : public detail::merc::base_merc_ellipsoid<T, Parameters>
    {
        inline merc_ellipsoid(const Parameters& par) : detail::merc::base_merc_ellipsoid<T, Parameters>(par)
        {
            detail::merc::setup_merc(this->m_par);
        }
    };

    /*!
        \brief Mercator projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Cylindrical
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_ts: Latitude of true scale (degrees)
        \par Example
        \image html ex_merc.gif
    */
    template <typename T, typename Parameters>
    struct merc_spheroid : public detail::merc::base_merc_spheroid<T, Parameters>
    {
        inline merc_spheroid(const Parameters& par) : detail::merc::base_merc_spheroid<T, Parameters>(par)
        {
            detail::merc::setup_merc(this->m_par);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::merc, merc_spheroid, merc_ellipsoid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class merc_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    if (par.es)
                        return new base_v_fi<merc_ellipsoid<T, Parameters>, T, Parameters>(par);
                    else
                        return new base_v_fi<merc_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void merc_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("merc", new merc_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_MERC_HPP

