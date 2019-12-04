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

#ifndef BOOST_GEOMETRY_PROJECTIONS_IGH_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IGH_HPP

#include <boost/geometry/util/math.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/proj/gn_sinu.hpp>
#include <boost/geometry/srs/projections/proj/moll.hpp>

namespace boost { namespace geometry
{

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace igh
    {
            // TODO: consider replacing dynamically created projections
            // with member objects
            template <typename T, typename Parameters>
            struct par_igh
            {
                boost::shared_ptr<base_v<T, Parameters> > pj[12];
                T dy0;
            };

            /* 40d 44' 11.8" [degrees] */
            template <typename T>
            inline T d4044118() { return (T(40) + T(44)/T(60.) + T(11.8)/T(3600.)) * geometry::math::d2r<T>(); }

            template <typename T>
            inline T d10() { return T(10) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d20() { return T(20) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d30() { return T(30) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d40() { return T(40) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d50() { return T(50) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d60() { return T(60) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d80() { return T(80) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d90() { return T(90) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d100() { return T(100) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d140() { return T(140) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d160() { return T(160) * geometry::math::d2r<T>(); }
            template <typename T>
            inline T d180() { return T(180) * geometry::math::d2r<T>(); }

            static const double epsilon = 1.e-10; // allow a little 'slack' on zone edge positions

            // Converted from #define SETUP(n, proj, x_0, y_0, lon_0)
            template <template <typename, typename, typename> class Entry, typename Params, typename Parameters, typename T>
            inline void do_setup(int n, Params const& params, Parameters const& par, par_igh<T, Parameters>& proj_parm,
                                 T const& x_0, T const& y_0,
                                 T const& lon_0)
            {
                // NOTE: in the original proj4 these projections are initialized
                // with zeroed parameters which could be done here as well instead
                // of initializing with parent projection's parameters.
                Entry<Params, T, Parameters> entry;
                proj_parm.pj[n-1].reset(entry.create_new(params, par));
                proj_parm.pj[n-1]->mutable_params().x0 = x_0;
                proj_parm.pj[n-1]->mutable_params().y0 = y_0;
                proj_parm.pj[n-1]->mutable_params().lam0 = lon_0;
            }

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_igh_spheroid
                : public base_t_fi<base_igh_spheroid<T, Parameters>, T, Parameters>
            {
                par_igh<T, Parameters> m_proj_parm;

                inline base_igh_spheroid(const Parameters& par)
                    : base_t_fi<base_igh_spheroid<T, Parameters>, T, Parameters>(*this, par)
                {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T lp_lon, T const& lp_lat, T& xy_x, T& xy_y) const
                {
                    static const T d4044118 = igh::d4044118<T>();
                    static const T d20  =  igh::d20<T>();
                    static const T d40  =  igh::d40<T>();
                    static const T d80  =  igh::d80<T>();
                    static const T d100 = igh::d100<T>();

                        int z;
                        if (lp_lat >=  d4044118) {          // 1|2
                          z = (lp_lon <= -d40 ? 1: 2);
                        }
                        else if (lp_lat >=  0) {            // 3|4
                          z = (lp_lon <= -d40 ? 3: 4);
                        }
                        else if (lp_lat >= -d4044118) {     // 5|6|7|8
                               if (lp_lon <= -d100) z =  5; // 5
                          else if (lp_lon <=  -d20) z =  6; // 6
                          else if (lp_lon <=   d80) z =  7; // 7
                          else z = 8;                       // 8
                        }
                        else {                              // 9|10|11|12
                               if (lp_lon <= -d100) z =  9; // 9
                          else if (lp_lon <=  -d20) z = 10; // 10
                          else if (lp_lon <=   d80) z = 11; // 11
                          else z = 12;                      // 12
                        }

                        lp_lon -= this->m_proj_parm.pj[z-1]->params().lam0;
                        this->m_proj_parm.pj[z-1]->fwd(lp_lon, lp_lat, xy_x, xy_y);
                        xy_x += this->m_proj_parm.pj[z-1]->params().x0;
                        xy_y += this->m_proj_parm.pj[z-1]->params().y0;
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T xy_x, T xy_y, T& lp_lon, T& lp_lat) const
                {
                    static const T d4044118 = igh::d4044118<T>();
                    static const T d10  =  igh::d10<T>();
                    static const T d20  =  igh::d20<T>();
                    static const T d40  =  igh::d40<T>();
                    static const T d50  =  igh::d50<T>();
                    static const T d60  =  igh::d60<T>();
                    static const T d80  =  igh::d80<T>();
                    static const T d90  =  igh::d90<T>();
                    static const T d100 = igh::d100<T>();
                    static const T d160 = igh::d160<T>();
                    static const T d180 = igh::d180<T>();

                    static const T c2 = 2.0;
                    
                    const T y90 = this->m_proj_parm.dy0 + sqrt(c2); // lt=90 corresponds to y=y0+sqrt(2.0)

                        int z = 0;
                        if (xy_y > y90+epsilon || xy_y < -y90+epsilon) // 0
                          z = 0;
                        else if (xy_y >=  d4044118)       // 1|2
                          z = (xy_x <= -d40? 1: 2);
                        else if (xy_y >=  0)              // 3|4
                          z = (xy_x <= -d40? 3: 4);
                        else if (xy_y >= -d4044118) {     // 5|6|7|8
                               if (xy_x <= -d100) z =  5; // 5
                          else if (xy_x <=  -d20) z =  6; // 6
                          else if (xy_x <=   d80) z =  7; // 7
                          else z = 8;                     // 8
                        }
                        else {                            // 9|10|11|12
                               if (xy_x <= -d100) z =  9; // 9
                          else if (xy_x <=  -d20) z = 10; // 10
                          else if (xy_x <=   d80) z = 11; // 11
                          else z = 12;                    // 12
                        }

                        if (z)
                        {
                          int ok = 0;

                          xy_x -= this->m_proj_parm.pj[z-1]->params().x0;
                          xy_y -= this->m_proj_parm.pj[z-1]->params().y0;
                          this->m_proj_parm.pj[z-1]->inv(xy_x, xy_y, lp_lon, lp_lat);
                          lp_lon += this->m_proj_parm.pj[z-1]->params().lam0;

                          switch (z) {
                            case  1: ok = (lp_lon >= -d180-epsilon && lp_lon <=  -d40+epsilon) ||
                                         ((lp_lon >=  -d40-epsilon && lp_lon <=  -d10+epsilon) &&
                                          (lp_lat >=   d60-epsilon && lp_lat <=   d90+epsilon)); break;
                            case  2: ok = (lp_lon >=  -d40-epsilon && lp_lon <=  d180+epsilon) ||
                                         ((lp_lon >= -d180-epsilon && lp_lon <= -d160+epsilon) &&
                                          (lp_lat >=   d50-epsilon && lp_lat <=   d90+epsilon)) ||
                                         ((lp_lon >=  -d50-epsilon && lp_lon <=  -d40+epsilon) &&
                                          (lp_lat >=   d60-epsilon && lp_lat <=   d90+epsilon)); break;
                            case  3: ok = (lp_lon >= -d180-epsilon && lp_lon <=  -d40+epsilon); break;
                            case  4: ok = (lp_lon >=  -d40-epsilon && lp_lon <=  d180+epsilon); break;
                            case  5: ok = (lp_lon >= -d180-epsilon && lp_lon <= -d100+epsilon); break;
                            case  6: ok = (lp_lon >= -d100-epsilon && lp_lon <=  -d20+epsilon); break;
                            case  7: ok = (lp_lon >=  -d20-epsilon && lp_lon <=   d80+epsilon); break;
                            case  8: ok = (lp_lon >=   d80-epsilon && lp_lon <=  d180+epsilon); break;
                            case  9: ok = (lp_lon >= -d180-epsilon && lp_lon <= -d100+epsilon); break;
                            case 10: ok = (lp_lon >= -d100-epsilon && lp_lon <=  -d20+epsilon); break;
                            case 11: ok = (lp_lon >=  -d20-epsilon && lp_lon <=   d80+epsilon); break;
                            case 12: ok = (lp_lon >=   d80-epsilon && lp_lon <=  d180+epsilon); break;
                          }

                          z = (!ok? 0: z); // projectable?
                        }
                     // if (!z) pj_errno = -15; // invalid x or y
                        if (!z) lp_lon = HUGE_VAL;
                        if (!z) lp_lat = HUGE_VAL;
                }

                static inline std::string get_name()
                {
                    return "igh_spheroid";
                }

            };

            // Interrupted Goode Homolosine
            template <typename Params, typename Parameters, typename T>
            inline void setup_igh(Params const& params, Parameters& par, par_igh<T, Parameters>& proj_parm)
            {
                static const T d0   =  0;
                static const T d4044118 = igh::d4044118<T>();
                static const T d20  =  igh::d20<T>();
                static const T d30  =  igh::d30<T>();
                static const T d60  =  igh::d60<T>();
                static const T d100 = igh::d100<T>();
                static const T d140 = igh::d140<T>();
                static const T d160 = igh::d160<T>();

            /*
              Zones:

                -180            -40                       180
                  +--------------+-------------------------+    Zones 1,2,9,10,11 & 12:
                  |1             |2                        |      Mollweide projection
                  |              |                         |
                  +--------------+-------------------------+    Zones 3,4,5,6,7 & 8:
                  |3             |4                        |      Sinusoidal projection
                  |              |                         |
                0 +-------+------+-+-----------+-----------+
                  |5      |6       |7          |8          |
                  |       |        |           |           |
                  +-------+--------+-----------+-----------+
                  |9      |10      |11         |12         |
                  |       |        |           |           |
                  +-------+--------+-----------+-----------+
                -180    -100      -20         80          180
            */
                
                    T lp_lam = 0, lp_phi = d4044118;
                    T xy1_x, xy1_y;
                    T xy3_x, xy3_y;

                    // IMPORTANT: Force spherical sinu projection
                    // This is required because unlike in the original proj4 here
                    // parameters are used to initialize underlying projections.
                    // In the original code zeroed parameters are passed which
                    // could be done here as well though.
                    par.es = 0.;

                    // sinusoidal zones
                    do_setup<sinu_entry>(3, params, par, proj_parm, -d100, d0, -d100);
                    do_setup<sinu_entry>(4, params, par, proj_parm,   d30, d0,   d30);
                    do_setup<sinu_entry>(5, params, par, proj_parm, -d160, d0, -d160);
                    do_setup<sinu_entry>(6, params, par, proj_parm,  -d60, d0,  -d60);
                    do_setup<sinu_entry>(7, params, par, proj_parm,   d20, d0,   d20);
                    do_setup<sinu_entry>(8, params, par, proj_parm,  d140, d0,  d140);

                    // mollweide zones
                    do_setup<moll_entry>(1, params, par, proj_parm, -d100, d0, -d100);

                    // y0 ?
                     proj_parm.pj[0]->fwd(lp_lam, lp_phi, xy1_x, xy1_y); // zone 1
                     proj_parm.pj[2]->fwd(lp_lam, lp_phi, xy3_x, xy3_y); // zone 3
                    // y0 + xy1_y = xy3_y for lt = 40d44'11.8"
                    proj_parm.dy0 = xy3_y - xy1_y;

                    proj_parm.pj[0]->mutable_params().y0 = proj_parm.dy0;

                    // mollweide zones (cont'd)
                    do_setup<moll_entry>( 2, params, par, proj_parm,   d30,  proj_parm.dy0,   d30);
                    do_setup<moll_entry>( 9, params, par, proj_parm, -d160, -proj_parm.dy0, -d160);
                    do_setup<moll_entry>(10, params, par, proj_parm,  -d60, -proj_parm.dy0,  -d60);
                    do_setup<moll_entry>(11, params, par, proj_parm,   d20, -proj_parm.dy0,   d20);
                    do_setup<moll_entry>(12, params, par, proj_parm,  d140, -proj_parm.dy0,  d140);

                    // Already done before
                    //par.es = 0.;
            }

    }} // namespace detail::igh
    #endif // doxygen

    /*!
        \brief Interrupted Goode Homolosine projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Pseudocylindrical
         - Spheroid
        \par Example
        \image html ex_igh.gif
    */
    template <typename T, typename Parameters>
    struct igh_spheroid : public detail::igh::base_igh_spheroid<T, Parameters>
    {
        template <typename Params>
        inline igh_spheroid(Params const& params, Parameters const& par)
            : detail::igh::base_igh_spheroid<T, Parameters>(par)
        {
            detail::igh::setup_igh(params, this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::spar::proj_igh, igh_spheroid, igh_spheroid)

        // Factory entry(s)
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_ENTRY_FI(igh_entry, igh_spheroid)

        BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_INIT_BEGIN(igh_init)
        {
            BOOST_GEOMETRY_PROJECTIONS_DETAIL_FACTORY_INIT_ENTRY(igh, igh_entry)
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_IGH_HPP

