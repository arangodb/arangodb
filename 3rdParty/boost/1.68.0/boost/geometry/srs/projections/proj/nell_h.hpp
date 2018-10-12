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

#ifndef BOOST_GEOMETRY_PROJECTIONS_NELL_H_HPP
#define BOOST_GEOMETRY_PROJECTIONS_NELL_H_HPP

#include <boost/geometry/util/math.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct nell_h {}; // Nell-Hammer

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace nell_h
    {

            static const int n_iter = 9;
            static const double epsilon = 1e-7;

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_nell_h_spheroid
                : public base_t_fi<base_nell_h_spheroid<T, Parameters>, T, Parameters>
            {
                inline base_nell_h_spheroid(const Parameters& par)
                    : base_t_fi<base_nell_h_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    xy_x = 0.5 * lp_lon * (1. + cos(lp_lat));
                    xy_y = 2.0 * (lp_lat - tan(0.5 *lp_lat));
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T half_pi = detail::half_pi<T>();

                    T V, c, p;
                    int i;

                    p = 0.5 * xy_y;
                    for (i = n_iter; i ; --i) {
                        c = cos(0.5 * lp_lat);
                        lp_lat -= V = (lp_lat - tan(lp_lat/2) - p)/(1. - 0.5/(c*c));
                        if (fabs(V) < epsilon)
                            break;
                    }
                    if (!i) {
                        lp_lat = p < 0. ? -half_pi : half_pi;
                        lp_lon = 2. * xy_x;
                    } else
                        lp_lon = 2. * xy_x / (1. + cos(lp_lat));
                }

                static inline std::string get_name()
                {
                    return "nell_h_spheroid";
                }

            };

            // Nell-Hammer
            template <typename Parameters>
            inline void setup_nell_h(Parameters& par)
            {
                par.es = 0.;
            }

    }} // namespace detail::nell_h
    #endif // doxygen

    /*!
        \brief Nell-Hammer projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_nell_h.gif
    */
    template <typename T, typename Parameters>
    struct nell_h_spheroid : public detail::nell_h::base_nell_h_spheroid<T, Parameters>
    {
        inline nell_h_spheroid(const Parameters& par) : detail::nell_h::base_nell_h_spheroid<T, Parameters>(par)
        {
            detail::nell_h::setup_nell_h(this->m_par);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::nell_h, nell_h_spheroid, nell_h_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class nell_h_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<nell_h_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void nell_h_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("nell_h", new nell_h_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_NELL_H_HPP

