// Boost.Geometry (aka GGL, Generic Geometry Library)
// This file is manually converted from PROJ4

// Copyright (c) 2008-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017, 2018.
// Modifications copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This file is converted from PROJ4, http://trac.osgeo.org/proj
// PROJ4 is originally written by Gerald Evenden (then of the USGS)
// PROJ4 is maintained by Frank Warmerdam
// PROJ4 is converted to Geometry Library by
//     Barend Gehrels (Geodan, Amsterdam)
//     Adam Wulkiewicz

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

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_ELL_SET_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_ELL_SET_HPP

#include <string>
#include <vector>

#include <boost/geometry/formulas/eccentricity_sqr.hpp>
#include <boost/geometry/util/math.hpp>

#include <boost/geometry/srs/projections/impl/pj_ellps.hpp>
#include <boost/geometry/srs/projections/impl/pj_param.hpp>
#include <boost/geometry/srs/projections/proj4.hpp>


namespace boost { namespace geometry { namespace projections {

namespace detail {

/* set ellipsoid parameters a and es */
template <typename T>
inline T SIXTH() { return .1666666666666666667; } /* 1/6 */
template <typename T>
inline T RA4() { return .04722222222222222222; } /* 17/360 */
template <typename T>
inline T RA6() { return .02215608465608465608; } /* 67/3024 */
template <typename T>
inline T RV4() { return .06944444444444444444; } /* 5/72 */
template <typename T>
inline T RV6() { return .04243827160493827160; } /* 55/1296 */

/* initialize geographic shape parameters */
template <typename BGParams, typename T>
inline void pj_ell_set(BGParams const& /*bg_params*/, std::vector<pvalue<T> >& parameters, T &a, T &es)
{
    T b = 0.0;
    T e = 0.0;
    std::string name;

    /* check for varying forms of ellipsoid input */
    a = es = 0.;

    /* R takes precedence */
    if (pj_param_f(parameters, "R", a)) {
        /* empty */
    } else { /* probable elliptical figure */

        /* check if ellps present and temporarily append its values to pl */
        name = pj_get_param_s(parameters, "ellps");
        if (! name.empty())
        {
            const int n = sizeof(pj_ellps) / sizeof(pj_ellps[0]);
            int index = -1;
            for (int i = 0; i < n && index == -1; i++)
            {
                if(pj_ellps[i].id == name)
                {
                    index = i;
                }
            }

            if (index == -1) {
                BOOST_THROW_EXCEPTION( projection_exception(error_unknown_ellp_param) );
            }

            pj_ellps_type const& pj_ellp = pj_ellps[index];
            parameters.push_back(pj_mkparam<T>("a", pj_ellp.major_v));
            parameters.push_back(pj_mkparam<T>(pj_ellp.ell_n, pj_ellp.ell_v));
        }
        a = pj_get_param_f(parameters, "a");
        if (pj_param_f(parameters, "es", es)) {/* eccentricity squared */
            /* empty */
        } else if (pj_param_f(parameters, "e", e)) { /* eccentricity */
            es = e * e;
        } else if (pj_param_f(parameters, "rf", es)) { /* recip flattening */
            if (!es) {
                BOOST_THROW_EXCEPTION( projection_exception(error_rev_flattening_is_zero) );
            }
            es = 1./ es;
            es = es * (2. - es);
        } else if (pj_param_f(parameters, "f", es)) { /* flattening */
            es = es * (2. - es);
        } else if (pj_param_f(parameters, "b", b)) { /* minor axis */
            es = 1. - (b * b) / (a * a);
        }     /* else es == 0. and sphere of radius a */
        if (!b)
            b = a * sqrt(1. - es);
        /* following options turn ellipsoid into equivalent sphere */
        if (pj_get_param_b(parameters, "R_A")) { /* sphere--area of ellipsoid */
            a *= 1. - es * (SIXTH<T>() + es * (RA4<T>() + es * RA6<T>()));
            es = 0.;
        } else if (pj_get_param_b(parameters, "R_V")) { /* sphere--vol. of ellipsoid */
            a *= 1. - es * (SIXTH<T>() + es * (RV4<T>() + es * RV6<T>()));
            es = 0.;
        } else if (pj_get_param_b(parameters, "R_a")) { /* sphere--arithmetic mean */
            a = .5 * (a + b);
            es = 0.;
        } else if (pj_get_param_b(parameters, "R_g")) { /* sphere--geometric mean */
            a = sqrt(a * b);
            es = 0.;
        } else if (pj_get_param_b(parameters, "R_h")) { /* sphere--harmonic mean */
            a = 2. * a * b / (a + b);
            es = 0.;
        } else {
            T tmp;
            int i = pj_param_r(parameters, "R_lat_a", tmp);
            if (i || /* sphere--arith. */
                pj_param_r(parameters, "R_lat_g", tmp)) { /* or geom. mean at latitude */

                tmp = sin(tmp);
                if (geometry::math::abs(tmp) > geometry::math::half_pi<T>()) {
                    BOOST_THROW_EXCEPTION( projection_exception(error_ref_rad_larger_than_90) );
                }
                tmp = 1. - es * tmp * tmp;
                a *= i ? .5 * (1. - es + tmp) / ( tmp * sqrt(tmp)) :
                    sqrt(1. - es) / tmp;
                es = 0.;
            }
        }
    }

    /* some remaining checks */
    if (es < 0.) {
        BOOST_THROW_EXCEPTION( projection_exception(error_es_less_than_zero) );
    }
    if (a <= 0.) {
        BOOST_THROW_EXCEPTION( projection_exception(error_major_axis_not_given) );
    }
}

template <BOOST_GEOMETRY_PROJECTIONS_DETAIL_TYPENAME_PX, typename T>
inline void pj_ell_set(srs::static_proj4<BOOST_GEOMETRY_PROJECTIONS_DETAIL_PX> const& bg_params,
                       std::vector<pvalue<T> >& /*parameters*/, T &a, T &es)
{
    typedef srs::static_proj4<BOOST_GEOMETRY_PROJECTIONS_DETAIL_PX> static_parameters_type;
    typedef typename srs::par4::detail::pick_ellps
        <
            static_parameters_type
        > pick_ellps;

    typename pick_ellps::model_type model = pick_ellps::model(bg_params);

    a = geometry::get_radius<0>(model);
    T b = geometry::get_radius<2>(model);
    es = 0.;
    if (a != b)
    {
        es = formula::eccentricity_sqr<T>(model);

        // Ignore all other parameters passed in string, at least for now
    }

    /* some remaining checks */
    if (es < 0.) {
        BOOST_THROW_EXCEPTION( projection_exception(error_es_less_than_zero) );
    }
    if (a <= 0.) {
        BOOST_THROW_EXCEPTION( projection_exception(error_major_axis_not_given) );
    }
}

template <typename T>
inline void pj_calc_ellipsoid_params(parameters<T> & p, T const& a, T const& es) {
/****************************************************************************************
    Calculate a large number of ancillary ellipsoidal parameters, in addition to
    the two traditional PROJ defining parameters: Semimajor axis, a, and the
    eccentricity squared, es.

    Most of these parameters are fairly cheap to compute in comparison to the overall
    effort involved in initializing a PJ object. They may, however, take a substantial
    part of the time taken in computing an individual point transformation.

    So by providing them up front, we can amortize the (already modest) cost over all
    transformations carried out over the entire lifetime of a PJ object, rather than
    incur that cost for every single transformation.

    Most of the parameter calculations here are based on the "angular eccentricity",
    i.e. the angle, measured from the semiminor axis, of a line going from the north
    pole to one of the foci of the ellipsoid - or in other words: The arc sine of the
    eccentricity.

    The formulae used are mostly taken from:

    Richard H. Rapp: Geometric Geodesy, Part I, (178 pp, 1991).
    Columbus, Ohio:  Dept. of Geodetic Science
    and Surveying, Ohio State University.

****************************************************************************************/

    p.a = a;
    p.es = es;

    /* Compute some ancillary ellipsoidal parameters */
    if (p.e==0)
        p.e = sqrt(p.es);  /* eccentricity */
    //p.alpha = asin (p.e);  /* angular eccentricity */

    /* second eccentricity */
    //p.e2  = tan (p.alpha);
    //p.e2s = p.e2 * p.e2;

    /* third eccentricity */
    //p.e3    = (0!=p.alpha)? sin (p.alpha) / sqrt(2 - sin (p.alpha)*sin (p.alpha)): 0;
    //p.e3s = p.e3 * p.e3;

    /* flattening */
    //if (0==p.f)
    //    p.f  = 1 - cos (p.alpha);   /* = 1 - sqrt (1 - PIN->es); */
    //p.rf = p.f != 0.0 ? 1.0/p.f: HUGE_VAL;

    /* second flattening */
    //p.f2  = (cos(p.alpha)!=0)? 1/cos (p.alpha) - 1: 0;
    //p.rf2 = p.f2 != 0.0 ? 1/p.f2: HUGE_VAL;

    /* third flattening */
    //p.n  = pow (tan (p.alpha/2), 2);
    //p.rn = p.n != 0.0 ? 1/p.n: HUGE_VAL;

    /* ...and a few more */
    //if (0==p.b)
    //    p.b  = (1 - p.f)*p.a;
    //p.rb = 1. / p.b;
    p.ra = 1. / p.a;

    p.one_es = 1. - p.es;
    if (p.one_es == 0.) {
        BOOST_THROW_EXCEPTION( projection_exception(error_eccentricity_is_one) );
    }

    p.rone_es = 1./p.one_es;
}


} // namespace detail
}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_ELL_SET_HPP
