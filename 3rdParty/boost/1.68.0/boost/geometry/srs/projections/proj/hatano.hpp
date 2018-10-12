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

#ifndef BOOST_GEOMETRY_PROJECTIONS_HATANO_HPP
#define BOOST_GEOMETRY_PROJECTIONS_HATANO_HPP

#include <boost/geometry/util/math.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct hatano {}; // Hatano Asymmetrical Equal Area

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace hatano
    {

            static const int n_iter = 20;
            static const double epsilon = 1e-7;
            static const double one_plus_tol = 1.000001;
            static const double CN_ = 2.67595;
            static const double CS_ = 2.43763;
            static const double RCN = 0.37369906014686373063;
            static const double RCS = 0.41023453108141924738;
            static const double FYCN = 1.75859;
            static const double FYCS = 1.93052;
            static const double RYCN = 0.56863737426006061674;
            static const double RYCS = 0.51799515156538134803;
            static const double FXC = 0.85;
            static const double RXC = 1.17647058823529411764;

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_hatano_spheroid
                : public base_t_fi<base_hatano_spheroid<T, Parameters>, T, Parameters>
            {
                inline base_hatano_spheroid(const Parameters& par)
                    : base_t_fi<base_hatano_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T th1, c;
                    int i;

                    c = sin(lp_lat) * (lp_lat < 0. ? CS_ : CN_);
                    for (i = n_iter; i; --i) {
                        lp_lat -= th1 = (lp_lat + sin(lp_lat) - c) / (1. + cos(lp_lat));
                        if (fabs(th1) < epsilon) break;
                    }
                    xy_x = FXC * lp_lon * cos(lp_lat *= .5);
                    xy_y = sin(lp_lat) * (lp_lat < 0. ? FYCS : FYCN);
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    static T const half_pi = detail::half_pi<T>();

                    T th;

                    th = xy_y * ( xy_y < 0. ? RYCS : RYCN);
                    if (fabs(th) > 1.) {
                        if (fabs(th) > one_plus_tol) {
                            BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                        } else {
                            th = th > 0. ? half_pi : - half_pi;
                        }
                    } else {
                        th = asin(th);
                    }

                    lp_lon = RXC * xy_x / cos(th);
                    th += th;
                    lp_lat = (th + sin(th)) * (xy_y < 0. ? RCS : RCN);
                    if (fabs(lp_lat) > 1.) {
                        if (fabs(lp_lat) > one_plus_tol) {
                            BOOST_THROW_EXCEPTION( projection_exception(error_tolerance_condition) );
                        } else {
                            lp_lat = lp_lat > 0. ? half_pi : - half_pi;
                        }
                    } else {
                        lp_lat = asin(lp_lat);
                    }
                }

                static inline std::string get_name()
                {
                    return "hatano_spheroid";
                }

            };

            // Hatano Asymmetrical Equal Area
            template <typename Parameters>
            inline void setup_hatano(Parameters& par)
            {
                par.es = 0.;
            }

    }} // namespace detail::hatano
    #endif // doxygen

    /*!
        \brief Hatano Asymmetrical Equal Area projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_hatano.gif
    */
    template <typename T, typename Parameters>
    struct hatano_spheroid : public detail::hatano::base_hatano_spheroid<T, Parameters>
    {
        inline hatano_spheroid(const Parameters& par) : detail::hatano::base_hatano_spheroid<T, Parameters>(par)
        {
            detail::hatano::setup_hatano(this->m_par);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::hatano, hatano_spheroid, hatano_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class hatano_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<hatano_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void hatano_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("hatano", new hatano_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_HATANO_HPP

