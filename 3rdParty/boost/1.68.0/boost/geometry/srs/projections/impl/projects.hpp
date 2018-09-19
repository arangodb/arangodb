// Boost.Geometry (aka GGL, Generic Geometry Library)
// This file is manually converted from PROJ4 (projects.h)

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
// PROJ4 is converted to Geometry Library by Barend Gehrels (Geodan, Amsterdam)

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

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_PROJECTS_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_PROJECTS_HPP


#include <cstring>
#include <string>
#include <vector>

#include <boost/config.hpp>
#include <boost/geometry/srs/projections/constants.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_pod.hpp>


namespace boost { namespace geometry { namespace projections
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

/* datum_type values */
enum datum_type
{
    datum_unknown   = 0,
    datum_3param    = 1,
    datum_7param    = 2,
    datum_gridshift = 3,
    datum_wgs84     = 4  /* WGS84 (or anything considered equivelent) */
};

/* library errors */
enum error_type
{
    error_no_args                 =  -1,
    error_no_option_in_init_file  =  -2,
    error_no_colon_in_init_string =  -3,
    error_proj_not_named          =  -4,
    error_unknown_projection_id   =  -5,
    error_eccentricity_is_one     =  -6,
    error_unknow_unit_id          =  -7,
    error_invalid_boolean_param   =  -8,
    error_unknown_ellp_param      =  -9,
    error_rev_flattening_is_zero  = -10,
    error_ref_rad_larger_than_90  = -11,
    error_es_less_than_zero       = -12,
    error_major_axis_not_given    = -13,
    error_lat_or_lon_exceed_limit = -14,
    error_invalid_x_or_y          = -15,
    error_wrong_format_dms_value  = -16,
    error_non_conv_inv_meri_dist  = -17,
    error_non_con_inv_phi2        = -18,
    error_acos_asin_arg_too_large = -19,
    error_tolerance_condition     = -20,
    error_conic_lat_equal         = -21,
    error_lat_larger_than_90      = -22,
    error_lat1_is_zero            = -23,
    error_lat_ts_larger_than_90   = -24,
    error_control_point_no_dist   = -25,
    error_no_rotation_proj        = -26,
    error_w_or_m_zero_or_less     = -27,
    error_lsat_not_in_range       = -28,
    error_path_not_in_range       = -29,
    error_h_less_than_zero        = -30,
    error_k_less_than_zero        = -31,
    error_lat_1_or_2_zero_or_90   = -32,
    error_lat_0_or_alpha_eq_90    = -33,
    error_ellipsoid_use_required  = -34,
    error_invalid_utm_zone        = -35,
    error_tcheby_val_out_of_range = -36,
    error_failed_to_find_proj     = -37,
    error_failed_to_load_grid     = -38,
    error_invalid_m_or_n          = -39,
    error_n_out_of_range          = -40,
    error_lat_1_2_unspecified     = -41,
    error_abs_lat1_eq_abs_lat2    = -42,
    error_lat_0_half_pi_from_mean = -43,
    error_unparseable_cs_def      = -44,
    error_geocentric              = -45,
    error_unknown_prime_meridian  = -46,
    error_axis                    = -47,
    error_grid_area               = -48,
    error_invalid_sweep_axis      = -49,
    error_malformed_pipeline      = -50,
    error_unit_factor_less_than_0 = -51,
    error_invalid_scale           = -52,
    error_non_convergent          = -53,
    error_missing_args            = -54,
    error_lat_0_is_zero           = -55,
    error_ellipsoidal_unsupported = -56,
    error_too_many_inits          = -57,
    error_invalid_arg             = -58
};

template <typename T>
struct pvalue
{
    std::string param;
    std::string s;
    //int used;
};

// Originally defined in proj_internal.h
//enum pj_io_units {
//    pj_io_units_whatever  = 0,  /* Doesn't matter (or depends on pipeline neighbours) */
//    pj_io_units_classic   = 1,  /* Scaled meters (right), projected system */
//    pj_io_units_projected = 2,  /* Meters, projected system */
//    pj_io_units_cartesian = 3,  /* Meters, 3D cartesian system */
//    pj_io_units_angular   = 4   /* Radians */
//};

// Originally defined in proj_internal.h
/* Maximum latitudinal overshoot accepted */
//static const double pj_epsilon_lat = 1e-12;

template <typename T>
struct pj_consts
{
    // E L L I P S O I D     P A R A M E T E R S

    T a;                            /* semimajor axis (radius if eccentricity==0) */
    T ra;                           /* 1/a */

    T e;                            /* first  eccentricity */
    T es;                           /* first  eccentricity squared */
    T one_es;                       /* 1 - e^2 */
    T rone_es;                      /* 1/one_es */

    T es_orig, a_orig;              /* es and a before any +proj related adjustment */

    // C O O R D I N A T E   H A N D L I N G

    int over;                       /* over-range flag */
    int geoc;                       /* geocentric latitude flag */
    int is_latlong;                 /* proj=latlong ... not really a projection at all */
    int is_geocent;                 /* proj=geocent ... not really a projection at all */
    //int need_ellps;                 /* 0 for operations that are purely cartesian */

    //enum pj_io_units left;          /* Flags for input/output coordinate types */
    //enum pj_io_units right;

    // C A R T O G R A P H I C       O F F S E T S

    T lam0, phi0;                   /* central longitude, latitude */
    T x0, y0/*, z0, t0*/;           /* false easting and northing (and height and time) */

    // S C A L I N G

    T k0;                           /* general scaling factor */
    T to_meter, fr_meter;           /* cartesian scaling */
    T vto_meter, vfr_meter;         /* Vertical scaling. Internal unit [m] */

    // D A T U M S   A N D   H E I G H T   S Y S T E M S    

    detail::datum_type datum_type;  /* PJD_UNKNOWN/3PARAM/7PARAM/GRIDSHIFT/WGS84 */
    T datum_params[7];              /* Parameters for 3PARAM and 7PARAM */

    T from_greenwich;               /* prime meridian offset (in radians) */
    T long_wrap_center;             /* 0.0 for -180 to 180, actually in radians*/
    bool is_long_wrap_set;

    // Initialize all variables
    pj_consts()
        : a(0), ra(0)
        , e(0), es(0), one_es(0), rone_es(0)
        , es_orig(0), a_orig(0)
        , over(0), geoc(0), is_latlong(0), is_geocent(0)
        //, need_ellps(1)
        //, left(PJ_IO_UNITS_ANGULAR), right(PJ_IO_UNITS_CLASSIC)
        , lam0(0), phi0(0)
        , x0(0), y0(0)/*, z0(0), t0(0)*/
        , k0(0) , to_meter(0), fr_meter(0), vto_meter(0), vfr_meter(0)
        , datum_type(datum_unknown)
#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) && (!defined(_MSC_VER) || (_MSC_VER >= 1900)) // workaround for VC++ 12 (aka 2013)
        , datum_params{0, 0, 0, 0, 0, 0, 0}
#endif
        , from_greenwich(0), long_wrap_center(0), is_long_wrap_set(false)
    {
#if defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX) || (defined(_MSC_VER) && (_MSC_VER < 1900)) // workaround for VC++ 12 (aka 2013)
        std::fill(datum_params, datum_params + 7, T(0));
#endif
    }
};

// PROJ4 complex. Might be replaced with std::complex
template <typename T>
struct pj_complex { T r, i; };

} // namespace detail
#endif // DOXYGEN_NO_DETAIL

/*!
    \brief parameters, projection parameters
    \details This structure initializes all projections
    \ingroup projection
*/
template <typename T>
struct parameters : public detail::pj_consts<T>
{
    typedef T type;

    std::string name;
    std::vector<detail::pvalue<T> > params;
};

}}} // namespace boost::geometry::projections
#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_PROJECTS_HPP
