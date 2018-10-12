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

#ifndef BOOST_GEOMETRY_PROJECTIONS_LASK_HPP
#define BOOST_GEOMETRY_PROJECTIONS_LASK_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct lask {}; // Laskowski

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace lask
    {

            static const double a10 = 0.975534;
            static const double a12 = -0.119161;
            static const double a32 = -0.0143059;
            static const double a14 = -0.0547009;
            static const double b01 = 1.00384;
            static const double b21 = 0.0802894;
            static const double b03 = 0.0998909;
            static const double b41 = 0.000199025;
            static const double b23 = -0.0285500;
            static const double b05 = -0.0491032;

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_lask_spheroid
                : public base_t_f<base_lask_spheroid<T, Parameters>, T, Parameters>
            {
                 inline base_lask_spheroid(const Parameters& par)
                    : base_t_f<base_lask_spheroid<T, Parameters>, T, Parameters>(*this, par)
                 {}

                // FORWARD(s_forward)  sphere
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T l2, p2;

                    l2 = lp_lon * lp_lon;
                    p2 = lp_lat * lp_lat;
                    xy_x = lp_lon * (a10 + p2 * (a12 + l2 * a32 + p2 * a14));
                    xy_y = lp_lat * (b01 + l2 * (b21 + p2 * b23 + l2 * b41) +
                               p2 * (b03 + p2 * b05));
                }

                static inline std::string get_name()
                {
                    return "lask_spheroid";
                }

            };

            // Laskowski
            template <typename Parameters>
            inline void setup_lask(Parameters& par)
            {
                par.es = 0.;
            }

    }} // namespace detail::lask
    #endif // doxygen

    /*!
        \brief Laskowski projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Miscellaneous
         - Spheroid
         - no inverse
        \par Example
        \image html ex_lask.gif
    */
    template <typename T, typename Parameters>
    struct lask_spheroid : public detail::lask::base_lask_spheroid<T, Parameters>
    {
        inline lask_spheroid(const Parameters& par) : detail::lask::base_lask_spheroid<T, Parameters>(par)
        {
            detail::lask::setup_lask(this->m_par);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::lask, lask_spheroid, lask_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class lask_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_f<lask_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void lask_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("lask", new lask_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_LASK_HPP

