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

#ifndef BOOST_GEOMETRY_PROJECTIONS_ECK2_HPP
#define BOOST_GEOMETRY_PROJECTIONS_ECK2_HPP

#include <boost/geometry/util/math.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct eck2 {};

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace eck2
    {

            static const double FXC = 0.46065886596178063902;
            static const double FYC = 1.44720250911653531871;
            //static const double C13 = 0.33333333333333333333;
            static const double one_plus_eps = 1.0000001;

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_eck2_spheroid
                : public base_t_fi<base_eck2_spheroid<T, Parameters>, T, Parameters>
            {
                inline base_eck2_spheroid(const Parameters& par)
                    : base_t_fi<base_eck2_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    xy_x = FXC * lp_lon * (xy_y = sqrt(4. - 3. * sin(fabs(lp_lat))));
                    xy_y = FYC * (2. - xy_y);
                    if ( lp_lat < 0.) xy_y = -xy_y;
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T half_pi = detail::half_pi<T>();
                    static const T C13 = detail::third<T>();

                    lp_lon = xy_x / (FXC * ( lp_lat = 2. - fabs(xy_y) / FYC) );
                    lp_lat = (4. - lp_lat * lp_lat) * C13;
                    if (fabs(lp_lat) >= 1.) {
                        if (fabs(lp_lat) > one_plus_eps)
                            BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                        else
                            lp_lat = lp_lat < 0. ? -half_pi : half_pi;
                    } else
                        lp_lat = asin(lp_lat);
                    if (xy_y < 0)
                        lp_lat = -lp_lat;
                }

                static inline std::string get_name()
                {
                    return "eck2_spheroid";
                }

            };

            // Eckert II
            template <typename Parameters>
            inline void setup_eck2(Parameters& par)
            {
                par.es = 0.;
            }

    }} // namespace detail::eck2
    #endif // doxygen

    /*!
        \brief Eckert II projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_eck2.gif
    */
    template <typename T, typename Parameters>
    struct eck2_spheroid : public detail::eck2::base_eck2_spheroid<T, Parameters>
    {
        inline eck2_spheroid(const Parameters& par) : detail::eck2::base_eck2_spheroid<T, Parameters>(par)
        {
            detail::eck2::setup_eck2(this->m_par);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::eck2, eck2_spheroid, eck2_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class eck2_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<eck2_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void eck2_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("eck2", new eck2_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_ECK2_HPP

