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

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_INIT_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_INIT_HPP

#include <cstdlib>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/range.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/geometry/util/math.hpp>
#include <boost/geometry/util/condition.hpp>

#include <boost/geometry/srs/projections/impl/dms_parser.hpp>
#include <boost/geometry/srs/projections/impl/pj_datum_set.hpp>
#include <boost/geometry/srs/projections/impl/pj_datums.hpp>
#include <boost/geometry/srs/projections/impl/pj_ell_set.hpp>
#include <boost/geometry/srs/projections/impl/pj_param.hpp>
#include <boost/geometry/srs/projections/impl/pj_units.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/proj4.hpp>


namespace boost { namespace geometry { namespace projections
{


namespace detail
{

template <typename BGParams, typename T>
inline void pj_push_defaults(BGParams const& /*bg_params*/, parameters<T>& pin)
{
    pin.params.push_back(pj_mkparam<T>("ellps", "WGS84"));

    if (pin.name == "aea")
    {
        pin.params.push_back(pj_mkparam<T>("lat_1", "29.5"));
        pin.params.push_back(pj_mkparam<T>("lat_2", "45.5 "));
    }
    else if (pin.name == "lcc")
    {
        pin.params.push_back(pj_mkparam<T>("lat_1", "33"));
        pin.params.push_back(pj_mkparam<T>("lat_2", "45"));
    }
    else if (pin.name == "lagrng")
    {
        pin.params.push_back(pj_mkparam<T>("W", "2"));
    }
}

template <BOOST_GEOMETRY_PROJECTIONS_DETAIL_TYPENAME_PX, typename T>
inline void pj_push_defaults(srs::static_proj4<BOOST_GEOMETRY_PROJECTIONS_DETAIL_PX> const& /*bg_params*/,
                             parameters<T>& pin)
{
    typedef srs::static_proj4<BOOST_GEOMETRY_PROJECTIONS_DETAIL_PX> static_parameters_type;
    typedef typename srs::par4::detail::pick_proj_tag
        <
            static_parameters_type
        >::type proj_tag;

    // statically defaulting to WGS84
    //pin.params.push_back(pj_mkparam("ellps", "WGS84"));

    if (BOOST_GEOMETRY_CONDITION((boost::is_same<proj_tag, srs::par4::aea>::value)))
    {
        pin.params.push_back(pj_mkparam<T>("lat_1", "29.5"));
        pin.params.push_back(pj_mkparam<T>("lat_2", "45.5 "));
    }
    else if (BOOST_GEOMETRY_CONDITION((boost::is_same<proj_tag, srs::par4::lcc>::value)))
    {
        pin.params.push_back(pj_mkparam<T>("lat_1", "33"));
        pin.params.push_back(pj_mkparam<T>("lat_2", "45"));
    }
    else if (BOOST_GEOMETRY_CONDITION((boost::is_same<proj_tag, srs::par4::lagrng>::value)))
    {
        pin.params.push_back(pj_mkparam<T>("W", "2"));
    }
}

template <typename T>
inline void pj_init_units(std::vector<pvalue<T> > const& params,
                          std::string const& sunits,
                          std::string const& sto_meter,
                          T & to_meter,
                          T & fr_meter,
                          T const& default_to_meter,
                          T const& default_fr_meter)
{
    std::string s;
    std::string units = pj_get_param_s(params, sunits);
    if (! units.empty())
    {
        const int n = sizeof(pj_units) / sizeof(pj_units[0]);
        int index = -1;
        for (int i = 0; i < n && index == -1; i++)
        {
            if(pj_units[i].id == units)
            {
                index = i;
            }
        }

        if (index == -1) {
            BOOST_THROW_EXCEPTION( projection_exception(error_unknow_unit_id) );
        }
        s = pj_units[index].to_meter;
    }

    if (s.empty())
    {
        s = pj_get_param_s(params, sto_meter);
    }

    if (! s.empty())
    {
        std::size_t const pos = s.find('/');
        if (pos == std::string::npos)
        {
            to_meter = geometry::str_cast<T>(s);
        }
        else
        {
            T const numerator = geometry::str_cast<T>(s.substr(0, pos));
            T const denominator = geometry::str_cast<T>(s.substr(pos + 1));
            if (numerator == 0.0 || denominator == 0.0)
            {
                BOOST_THROW_EXCEPTION( projection_exception(error_unit_factor_less_than_0) );
            }
            to_meter = numerator / denominator;
        }
        if (to_meter == 0.0)
        {
            BOOST_THROW_EXCEPTION( projection_exception(error_unit_factor_less_than_0) );
        }
        fr_meter = 1. / to_meter;
    }
    else
    {
        to_meter = default_to_meter;
        fr_meter = default_fr_meter;
    }
}

/************************************************************************/
/*                              pj_init()                               */
/*                                                                      */
/*      Main entry point for initialing a PJ projections                */
/*      definition.  Note that the projection specific function is      */
/*      called to do the initial allocation so it can be created        */
/*      large enough to hold projection specific parameters.            */
/************************************************************************/
template <typename T, typename BGParams, typename R>
inline parameters<T> pj_init(BGParams const& bg_params, R const& arguments, bool use_defaults = true)
{
    parameters<T> pin;
    for (std::vector<std::string>::const_iterator it = boost::begin(arguments);
        it != boost::end(arguments); it++)
    {
        pin.params.push_back(pj_mkparam<T>(*it));
    }

    // maybe TODO: handle "init" parameter
    /* check if +init present */
    //std::string sinit;
    //if (pj_param_s(pin.params, "init", sinit))
    //{
    //    //if (!(curr = get_init(&arguments, curr, sinit)))
    //}

    // find projection -> implemented in projection factory
    pin.name = pj_get_param_s(pin.params, "proj");
    // exception thrown in projection<>
    // TODO: consider throwing here both projection_unknown_id_exception and
    // projection_not_named_exception in order to throw before other exceptions
    //if (pin.name.empty())
    //{ BOOST_THROW_EXCEPTION( projection_not_named_exception() ); }

    // set defaults, unless inhibited
    // GL-Addition, if use_defaults is false then defaults are ignored
    if (use_defaults && ! pj_get_param_b(pin.params, "no_defs"))
    {
        // proj4 gets defaults from "proj_def.dat", file of 94/02/23 with a few defaults.
        // Here manually
        pj_push_defaults(bg_params, pin);
        //curr = get_defaults(&arguments, curr, name);
    }

    /* allocate projection structure */
    // done by BGParams constructor:
    // pin.is_latlong = 0;
    // pin.is_geocent = 0;
    // pin.long_wrap_center = 0.0;
    // pin.long_wrap_center = 0.0;
    pin.is_long_wrap_set = false;

    /* set datum parameters */
    pj_datum_set(bg_params, pin.params, pin);

    /* set ellipsoid/sphere parameters */
    pj_ell_set(bg_params, pin.params, pin.a, pin.es);

    pin.a_orig = pin.a;
    pin.es_orig = pin.es;

    pin.e = sqrt(pin.es);
    pin.ra = 1. / pin.a;
    pin.one_es = 1. - pin.es;
    if (pin.one_es == 0.) {
        BOOST_THROW_EXCEPTION( projection_exception(error_eccentricity_is_one) );
    }
    pin.rone_es = 1./pin.one_es;

    /* Now that we have ellipse information check for WGS84 datum */
    if( pin.datum_type == datum_3param
        && pin.datum_params[0] == 0.0
        && pin.datum_params[1] == 0.0
        && pin.datum_params[2] == 0.0
        && pin.a == 6378137.0
        && geometry::math::abs(pin.es - 0.006694379990) < 0.000000000050 )/*WGS84/GRS80*/
    {
        pin.datum_type = datum_wgs84;
    }

    /* set pin.geoc coordinate system */
    pin.geoc = (pin.es && pj_get_param_b(pin.params, "geoc"));

    /* over-ranging flag */
    pin.over = pj_get_param_b(pin.params, "over");

    /* longitude center for wrapping */
    pin.is_long_wrap_set = pj_param_r(pin.params, "lon_wrap", pin.long_wrap_center);

    /* central meridian */
    pin.lam0 = pj_get_param_r(pin.params, "lon_0");

    /* central latitude */
    pin.phi0 = pj_get_param_r(pin.params, "lat_0");

    /* false easting and northing */
    pin.x0 = pj_get_param_f(pin.params, "x_0");
    pin.y0 = pj_get_param_f(pin.params, "y_0");

    /* general scaling factor */
    if (pj_param_f(pin.params, "k_0", pin.k0)) {
        /* empty */
    } else if (pj_param_f(pin.params, "k", pin.k0)) {
        /* empty */
    } else
        pin.k0 = 1.;
    if (pin.k0 <= 0.) {
        BOOST_THROW_EXCEPTION( projection_exception(error_k_less_than_zero) );
    }

    /* set units */
    pj_init_units(pin.params, "units", "to_meter",
                  pin.to_meter, pin.fr_meter, 1., 1.);
    pj_init_units(pin.params, "vunits", "vto_meter",
                  pin.vto_meter, pin.vfr_meter, pin.to_meter, pin.fr_meter);

    /* prime meridian */
    std::string pm = pj_get_param_s(pin.params, "pm");
    if (! pm.empty())
    {
        std::string value;

        int n = sizeof(pj_prime_meridians) / sizeof(pj_prime_meridians[0]);
        for (int i = 0; i < n ; i++)
        {
            if(pj_prime_meridians[i].id == pm)
            {
                value = pj_prime_meridians[i].defn;
                break;
            }
        }

        dms_parser<T, true> parser;

        // TODO: Is this try-catch needed?
        // In other cases the bad_str_cast exception is simply thrown
        BOOST_TRY
        {
            if (value.empty()) {
                pin.from_greenwich = parser.apply(pm).angle();
            } else {
                pin.from_greenwich = parser.apply(value).angle();
            }
        }
        BOOST_CATCH(geometry::bad_str_cast const&)
        {
            BOOST_THROW_EXCEPTION( projection_exception(error_unknown_prime_meridian) );
        }
        BOOST_CATCH_END
    }
    else
    {
        pin.from_greenwich = 0.0;
    }

    return pin;
}

/************************************************************************/
/*                            pj_init_plus()                            */
/*                                                                      */
/*      Same as pj_init() except it takes one argument string with      */
/*      individual arguments preceeded by '+', such as "+proj=utm       */
/*      +zone=11 +ellps=WGS84".                                         */
/************************************************************************/
template <typename T, typename BGParams>
inline parameters<T> pj_init_plus(BGParams const& bg_params, std::string const& definition, bool use_defaults = true)
{
    const char* sep = " +";

    /* split into arguments based on '+' and trim white space */

    // boost::split splits on one character, here it should be on " +", so implementation below
    // todo: put in different routine or sort out
    std::vector<std::string> arguments;
    std::string def = boost::trim_copy(definition);
    boost::trim_left_if(def, boost::is_any_of(sep));

    std::string::size_type loc = def.find(sep);
    while (loc != std::string::npos)
    {
        std::string par = def.substr(0, loc);
        boost::trim(par);
        if (! par.empty())
        {
            arguments.push_back(par);
        }

        def.erase(0, loc);
        boost::trim_left_if(def, boost::is_any_of(sep));
        loc = def.find(sep);
    }

    if (! def.empty())
    {
        arguments.push_back(def);
    }

    /*boost::split(arguments, definition, boost::is_any_of("+"));
    for (std::vector<std::string>::iterator it = arguments.begin(); it != arguments.end(); it++)
    {
        boost::trim(*it);
    }*/
    return pj_init<T>(bg_params, arguments, use_defaults);
}

} // namespace detail
}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_INIT_HPP
