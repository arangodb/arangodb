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

#ifndef BOOST_GEOMETRY_PROJECTIONS_OB_TRAN_HPP
#define BOOST_GEOMETRY_PROJECTIONS_OB_TRAN_HPP

#include <boost/geometry/util/math.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>
#include <boost/geometry/srs/projections/impl/aasincos.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    //struct ob_tran_oblique {};
    //struct ob_tran_transverse {};
    struct ob_tran {}; // General Oblique Transformation

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail {
    
        // fwd declaration needed below
        template <typename T>
        inline detail::base_v<T, parameters<T> >*
            create_new(parameters<T> const& parameters);

    } // namespace detail

    namespace detail { namespace ob_tran
    {

            static const double tolerance = 1e-10;

            template <typename Parameters>
            inline Parameters o_proj_parameters(Parameters const& par)
            {
                /* copy existing header into new */
                Parameters pj = par;

                /* get name of projection to be translated */
                pj.name = pj_get_param_s(par.params, "o_proj");
                if (pj.name.empty())
                    BOOST_THROW_EXCEPTION( projection_exception(error_no_rotation_proj) );

                /* avoid endless recursion */
                if( pj.name == "ob_tran")
                    BOOST_THROW_EXCEPTION( projection_exception(error_failed_to_find_proj) );

                /* force spherical earth */
                pj.one_es = pj.rone_es = 1.;
                pj.es = pj.e = 0.;

                return pj;
            }

            template <typename T, typename Parameters>
            struct par_ob_tran
            {
                par_ob_tran(Parameters const& par)
                    : link(projections::detail::create_new(o_proj_parameters(par)))
                {
                    if (! link.get())
                        BOOST_THROW_EXCEPTION( projection_exception(error_unknown_projection_id) );
                }

                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    link->fwd(lp_lon, lp_lat, xy_x, xy_y);
                }

                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    link->inv(xy_x, xy_y, lp_lon, lp_lat);
                }

                boost::shared_ptr<base_v<T, Parameters> > link;
                T lamp;
                T cphip, sphip;
            };

            template <typename StaticParameters, typename T, typename Parameters>
            struct par_ob_tran_static
            {
                // this metafunction handles static error handling
                typedef typename srs::par4::detail::pick_o_proj_tag
                    <
                        StaticParameters
                    >::type o_proj_tag;

                /* avoid endless recursion */
                static const bool is_o_proj_not_ob_tran = ! boost::is_same<o_proj_tag, srs::par4::ob_tran>::value;
                BOOST_MPL_ASSERT_MSG((is_o_proj_not_ob_tran), INVALID_O_PROJ_PARAMETER, (StaticParameters));

                typedef typename projections::detail::static_projection_type
                    <
                        o_proj_tag,
                        srs_sphere_tag, // force spherical
                        StaticParameters,
                        T,
                        Parameters
                    >::type projection_type;

                par_ob_tran_static(Parameters const& par)
                    : link(o_proj_parameters(par))
                {}

                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    link.fwd(lp_lon, lp_lat, xy_x, xy_y);
                }

                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    link.inv(xy_x, xy_y, lp_lon, lp_lat);
                }

                projection_type link;
                T lamp;
                T cphip, sphip;
            };

            template <typename T, typename Par>
            inline void o_forward(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y, Par const& proj_parm)
            {
                T coslam, sinphi, cosphi;
                
                coslam = cos(lp_lon);
                sinphi = sin(lp_lat);
                cosphi = cos(lp_lat);
                lp_lon = adjlon(aatan2(cosphi * sin(lp_lon), proj_parm.sphip * cosphi * coslam +
                    proj_parm.cphip * sinphi) + proj_parm.lamp);
                lp_lat = aasin(proj_parm.sphip * sinphi - proj_parm.cphip * cosphi * coslam);

                proj_parm.fwd(lp_lon, lp_lat, xy_x, xy_y);
            }

            template <typename T, typename Par>
            inline void o_inverse(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat, Par const& proj_parm)
            {
                T coslam, sinphi, cosphi;

                proj_parm.inv(xy_x, xy_y, lp_lon, lp_lat);
                if (lp_lon != HUGE_VAL) {
                    coslam = cos(lp_lon -= proj_parm.lamp);
                    sinphi = sin(lp_lat);
                    cosphi = cos(lp_lat);
                    lp_lat = aasin(proj_parm.sphip * sinphi + proj_parm.cphip * cosphi * coslam);
                    lp_lon = aatan2(cosphi * sin(lp_lon), proj_parm.sphip * cosphi * coslam -
                        proj_parm.cphip * sinphi);
                }
            }

            template <typename T, typename Par>
            inline void t_forward(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y, Par const& proj_parm)
            {
                T cosphi, coslam;

                cosphi = cos(lp_lat);
                coslam = cos(lp_lon);
                lp_lon = adjlon(aatan2(cosphi * sin(lp_lon), sin(lp_lat)) + proj_parm.lamp);
                lp_lat = aasin(- cosphi * coslam);

                proj_parm.fwd(lp_lon, lp_lat, xy_x, xy_y);
            }

            template <typename T, typename Par>
            inline void t_inverse(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat, Par const& proj_parm)
            {
                T cosphi, t;

                proj_parm.inv(xy_x, xy_y, lp_lon, lp_lat);
                if (lp_lon != HUGE_VAL) {
                    cosphi = cos(lp_lat);
                    t = lp_lon - proj_parm.lamp;
                    lp_lon = aatan2(cosphi * sin(t), - sin(lp_lat));
                    lp_lat = aasin(cosphi * cos(t));
                }
            }

            // General Oblique Transformation
            template <typename T, typename Parameters, typename ProjParameters>
            inline T setup_ob_tran(Parameters & par, ProjParameters& proj_parm)
            {
                static const T half_pi = detail::half_pi<T>();

                T phip, alpha;

                par.es = 0.; /* force to spherical */

                // proj_parm.link should be created at this point

                if (pj_param_r(par.params, "o_alpha", alpha)) {
                    T lamc, phic;

                    lamc    = pj_get_param_r(par.params, "o_lon_c");
                    phic    = pj_get_param_r(par.params, "o_lat_c");
                    //alpha   = pj_get_param_r(par.params, "o_alpha");
            
                    if (fabs(fabs(phic) - half_pi) <= tolerance)
                        BOOST_THROW_EXCEPTION( projection_exception(error_lat_0_or_alpha_eq_90) );

                    proj_parm.lamp = lamc + aatan2(-cos(alpha), -sin(alpha) * sin(phic));
                    phip = aasin(cos(phic) * sin(alpha));
                } else if (pj_param_r(par.params, "o_lat_p", phip)) { /* specified new pole */
                    proj_parm.lamp = pj_get_param_r(par.params, "o_lon_p");
                    //phip = pj_param_r(par.params, "o_lat_p");
                } else { /* specified new "equator" points */
                    T lam1, lam2, phi1, phi2, con;

                    lam1 = pj_get_param_r(par.params, "o_lon_1");
                    phi1 = pj_get_param_r(par.params, "o_lat_1");
                    lam2 = pj_get_param_r(par.params, "o_lon_2");
                    phi2 = pj_get_param_r(par.params, "o_lat_2");
                    if (fabs(phi1 - phi2) <= tolerance || (con = fabs(phi1)) <= tolerance ||
                        fabs(con - half_pi) <= tolerance || fabs(fabs(phi2) - half_pi) <= tolerance)
                        BOOST_THROW_EXCEPTION( projection_exception(error_lat_1_or_2_zero_or_90) );

                    proj_parm.lamp = atan2(cos(phi1) * sin(phi2) * cos(lam1) -
                        sin(phi1) * cos(phi2) * cos(lam2),
                        sin(phi1) * cos(phi2) * sin(lam2) -
                        cos(phi1) * sin(phi2) * sin(lam1));
                    phip = atan(-cos(proj_parm.lamp - lam1) / tan(phi1));
                }

                if (fabs(phip) > tolerance) { /* oblique */
                    proj_parm.cphip = cos(phip);
                    proj_parm.sphip = sin(phip);
                } else { /* transverse */
                }

                // TODO:
                /* Support some rather speculative test cases, where the rotated projection */
                /* is actually latlong. We do not want scaling in that case... */
                //if (proj_parm.link...mutable_parameters().right==PJ_IO_UNITS_ANGULAR)
                //    par.right = PJ_IO_UNITS_PROJECTED;

                // return phip to choose model
                return phip;
            }

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_ob_tran_oblique
                : public base_t_fi<base_ob_tran_oblique<T, Parameters>, T, Parameters>
            {
                par_ob_tran<T, Parameters> m_proj_parm;

                inline base_ob_tran_oblique(Parameters const& par,
                                            par_ob_tran<T, Parameters> const& proj_parm)
                    : base_t_fi
                        <
                            base_ob_tran_oblique<T, Parameters>, T, Parameters
                        >(*this, par)
                    , m_proj_parm(proj_parm)
                {}

                // FORWARD(o_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    o_forward(lp_lon, lp_lat, xy_x, xy_y, this->m_proj_parm);
                }

                // INVERSE(o_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    o_inverse(xy_x, xy_y, lp_lon, lp_lat, this->m_proj_parm);
                }

                static inline std::string get_name()
                {
                    return "ob_tran_oblique";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename T, typename Parameters>
            struct base_ob_tran_transverse
                : public base_t_fi<base_ob_tran_transverse<T, Parameters>, T, Parameters>
            {
                par_ob_tran<T, Parameters> m_proj_parm;

                inline base_ob_tran_transverse(Parameters const& par,
                                               par_ob_tran<T, Parameters> const& proj_parm)
                    : base_t_fi
                        <
                            base_ob_tran_transverse<T, Parameters>, T, Parameters
                        >(*this, par)
                    , m_proj_parm(proj_parm)
                {}

                // FORWARD(t_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    t_forward(lp_lon, lp_lat, xy_x, xy_y, this->m_proj_parm);
                }

                // INVERSE(t_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    t_inverse(xy_x, xy_y, lp_lon, lp_lat, this->m_proj_parm);
                }

                static inline std::string get_name()
                {
                    return "ob_tran_transverse";
                }

            };

            // template class, using CRTP to implement forward/inverse
            template <typename StaticParameters, typename T, typename Parameters>
            struct base_ob_tran_static
                : public base_t_fi<base_ob_tran_static<StaticParameters, T, Parameters>, T, Parameters>
            {
                par_ob_tran_static<StaticParameters, T, Parameters> m_proj_parm;
                bool m_is_oblique;

                inline base_ob_tran_static(Parameters const& par)
                    : base_t_fi<base_ob_tran_static<StaticParameters, T, Parameters>, T, Parameters>(*this, par)
                    , m_proj_parm(par)
                {}

                // FORWARD(o_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(T& lp_lon, T& lp_lat, T& xy_x, T& xy_y) const
                {
                    if (m_is_oblique) {
                        o_forward(lp_lon, lp_lat, xy_x, xy_y, this->m_proj_parm);
                    } else {
                        t_forward(lp_lon, lp_lat, xy_x, xy_y, this->m_proj_parm);
                    }
                }

                // INVERSE(o_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(T& xy_x, T& xy_y, T& lp_lon, T& lp_lat) const
                {
                    if (m_is_oblique) {
                        o_inverse(xy_x, xy_y, lp_lon, lp_lat, this->m_proj_parm);
                    } else {
                        t_inverse(xy_x, xy_y, lp_lon, lp_lat, this->m_proj_parm);
                    }
                }

                static inline std::string get_name()
                {
                    return "ob_tran";
                }

            };

    }} // namespace detail::ob_tran
    #endif // doxygen

    /*!
        \brief General Oblique Transformation projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Miscellaneous
         - Spheroid
        \par Projection parameters
         - o_proj (string)
         - Plus projection parameters
         - o_lat_p (degrees)
         - o_lon_p (degrees)
         - New pole
         - o_alpha: Alpha (degrees)
         - o_lon_c (degrees)
         - o_lat_c (degrees)
         - o_lon_1 (degrees)
         - o_lat_1: Latitude of first standard parallel (degrees)
         - o_lon_2 (degrees)
         - o_lat_2: Latitude of second standard parallel (degrees)
        \par Example
        \image html ex_ob_tran.gif
    */
    template <typename T, typename Parameters>
    struct ob_tran_oblique : public detail::ob_tran::base_ob_tran_oblique<T, Parameters>
    {
        inline ob_tran_oblique(Parameters const& par,
                               detail::ob_tran::par_ob_tran<T, Parameters> const& proj_parm)
            : detail::ob_tran::base_ob_tran_oblique<T, Parameters>(par, proj_parm)
        {
            // already done
            //detail::ob_tran::setup_ob_tran(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief General Oblique Transformation projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Miscellaneous
         - Spheroid
        \par Projection parameters
         - o_proj (string)
         - Plus projection parameters
         - o_lat_p (degrees)
         - o_lon_p (degrees)
         - New pole
         - o_alpha: Alpha (degrees)
         - o_lon_c (degrees)
         - o_lat_c (degrees)
         - o_lon_1 (degrees)
         - o_lat_1: Latitude of first standard parallel (degrees)
         - o_lon_2 (degrees)
         - o_lat_2: Latitude of second standard parallel (degrees)
        \par Example
        \image html ex_ob_tran.gif
    */
    template <typename T, typename Parameters>
    struct ob_tran_transverse : public detail::ob_tran::base_ob_tran_transverse<T, Parameters>
    {
        inline ob_tran_transverse(Parameters const& par,
                                  detail::ob_tran::par_ob_tran<T, Parameters> const& proj_parm)
            : detail::ob_tran::base_ob_tran_transverse<T, Parameters>(par, proj_parm)
        {
            // already done
            //detail::ob_tran::setup_ob_tran(this->m_par, this->m_proj_parm);
        }
    };

    /*!
        \brief General Oblique Transformation projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Miscellaneous
         - Spheroid
        \par Projection parameters
         - o_proj (string)
         - Plus projection parameters
         - o_lat_p (degrees)
         - o_lon_p (degrees)
         - New pole
         - o_alpha: Alpha (degrees)
         - o_lon_c (degrees)
         - o_lat_c (degrees)
         - o_lon_1 (degrees)
         - o_lat_1: Latitude of first standard parallel (degrees)
         - o_lon_2 (degrees)
         - o_lat_2: Latitude of second standard parallel (degrees)
        \par Example
        \image html ex_ob_tran.gif
    */
    template <typename StaticParameters, typename T, typename Parameters>
    struct ob_tran_static : public detail::ob_tran::base_ob_tran_static<StaticParameters, T, Parameters>
    {
        inline ob_tran_static(const Parameters& par)
            : detail::ob_tran::base_ob_tran_static<StaticParameters, T, Parameters>(par)
        {
            T phip = detail::ob_tran::setup_ob_tran<T>(this->m_par, this->m_proj_parm);
            this->m_is_oblique = fabs(phip) > detail::ob_tran::tolerance;
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        template <typename BGP, typename CT, typename P>
        struct static_projection_type<srs::par4::ob_tran, srs_sphere_tag, BGP, CT, P>
        {
            typedef ob_tran_static<BGP, CT, P> type;
        };
        template <typename BGP, typename CT, typename P>
        struct static_projection_type<srs::par4::ob_tran, srs_spheroid_tag, BGP, CT, P>
        {
            typedef ob_tran_static<BGP, CT, P> type;
        };

        // Factory entry(s)
        template <typename T, typename Parameters>
        class ob_tran_entry : public detail::factory_entry<T, Parameters>
        {
            public :
                virtual base_v<T, Parameters>* create_new(const Parameters& par) const
                {
                    Parameters params = par;
                    detail::ob_tran::par_ob_tran<T, Parameters> proj_parm(params);
                    T phip = detail::ob_tran::setup_ob_tran<T>(params, proj_parm);

                    if (fabs(phip) > detail::ob_tran::tolerance)
                        return new base_v_fi<ob_tran_oblique<T, Parameters>, T, Parameters>(params, proj_parm);
                    else
                        return new base_v_fi<ob_tran_transverse<T, Parameters>, T, Parameters>(params, proj_parm);
                }
        };

        template <typename T, typename Parameters>
        inline void ob_tran_init(detail::base_factory<T, Parameters>& factory)
        {
            factory.add_to_factory("ob_tran", new ob_tran_entry<T, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_OB_TRAN_HPP

