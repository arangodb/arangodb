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

#ifndef BOOST_GEOMETRY_PROJECTIONS_GSTMERC_HPP
#define BOOST_GEOMETRY_PROJECTIONS_GSTMERC_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/pj_phi2.hpp>
#include <boost/geometry/srs/projections/impl/pj_tsfn.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct gstmerc {}; // Gauss-Schreiber Transverse Mercator (aka Gauss-Laborde Reunion)

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace gstmerc
    {
            template <typename T>
            struct par_gstmerc
            {
                T lamc;
                T phic;
                T c;
                T n1;
                T n2;
                T XS;
                T YS;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_gstmerc_spheroid
                : public base_t_fi<base_gstmerc_spheroid<T, Parameters>, T, Parameters>
            {
                par_gstmerc<T> m_proj_parm;

                inline base_gstmerc_spheroid(const Parameters& par)
                    : base_t_fi<base_gstmerc_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    T L, Ls, sinLs1, Ls1;

                    L= this->m_proj_parm.n1*lp_lon;
                    Ls= this->m_proj_parm.c+this->m_proj_parm.n1*log(pj_tsfn(-1.0*lp_lat,-1.0*sin(lp_lat),this->m_par.e));
                    sinLs1= sin(L)/cosh(Ls);
                    Ls1= log(pj_tsfn(-1.0*asin(sinLs1),0.0,0.0));
                    xy_x= (this->m_proj_parm.XS + this->m_proj_parm.n2*Ls1)*this->m_par.ra;
                    xy_y= (this->m_proj_parm.YS + this->m_proj_parm.n2*atan(sinh(Ls)/cos(L)))*this->m_par.ra;
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    T L, LC, sinC;

                    L= atan(sinh((xy_x*this->m_par.a - this->m_proj_parm.XS)/this->m_proj_parm.n2)/cos((xy_y*this->m_par.a - this->m_proj_parm.YS)/this->m_proj_parm.n2));
                    sinC= sin((xy_y*this->m_par.a - this->m_proj_parm.YS)/this->m_proj_parm.n2)/cosh((xy_x*this->m_par.a - this->m_proj_parm.XS)/this->m_proj_parm.n2);
                    LC= log(pj_tsfn(-1.0*asin(sinC),0.0,0.0));
                    lp_lon= L/this->m_proj_parm.n1;
                    lp_lat= -1.0*pj_phi2(exp((LC-this->m_proj_parm.c)/this->m_proj_parm.n1),this->m_par.e);
                }

                static inline std::string get_name()
                {
                    return "gstmerc_spheroid";
                }

            };

            // Gauss-Schreiber Transverse Mercator (aka Gauss-Laborde Reunion)
            template <typename Parameters, typename T>
            inline void setup_gstmerc(Parameters& par, par_gstmerc<T>& proj_parm)
            {
                proj_parm.lamc= par.lam0;
                proj_parm.n1= sqrt(T(1)+par.es*math::pow(cos(par.phi0),4)/(T(1)-par.es));
                proj_parm.phic= asin(sin(par.phi0)/proj_parm.n1);
                proj_parm.c= log(pj_tsfn(-1.0*proj_parm.phic,0.0,0.0))
                           - proj_parm.n1*log(pj_tsfn(-1.0*par.phi0,-1.0*sin(par.phi0),par.e));
                proj_parm.n2= par.k0*par.a*sqrt(1.0-par.es)/(1.0-par.es*sin(par.phi0)*sin(par.phi0));
                proj_parm.XS= 0;/* -par.x0 */
                proj_parm.YS= -1.0*proj_parm.n2*proj_parm.phic;/* -par.y0 */
            }

    }} // namespace detail::gstmerc
    #endif // doxygen

    /*!
        \brief Gauss-Schreiber Transverse Mercator (aka Gauss-Laborde Reunion) projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Cylindrical
         - Spheroid
         - Ellipsoid
        \par Projection parameters
         - lat_0: Latitude of origin
         - lon_0: Central meridian
         - k_0: Scale factor
        \par Example
        \image html ex_gstmerc.gif
    */
    template <typename T, typename Parameters>
    struct gstmerc_spheroid : public detail::gstmerc::base_gstmerc_spheroid<T, Parameters>
    {
        inline gstmerc_spheroid(const Parameters& par) : detail::gstmerc::base_gstmerc_spheroid<T, Parameters>(par)
        {
            detail::gstmerc::setup_gstmerc(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::gstmerc, gstmerc_spheroid, gstmerc_spheroid)

        // Factory entry(s)
        template <typename T, typename Parameters>
        class gstmerc_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<gstmerc_spheroid<T, Parameters>, T, Parameters>(par);
                }
        };

        template <typename T, typename Parameters>
        inline void gstmerc_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("gstmerc", new gstmerc_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_GSTMERC_HPP

