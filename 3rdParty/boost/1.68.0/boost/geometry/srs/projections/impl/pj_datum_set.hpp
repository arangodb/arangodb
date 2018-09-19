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

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_DATUM_SET_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_DATUM_SET_HPP


#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include <boost/geometry/srs/projections/exception.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/pj_datums.hpp>
#include <boost/geometry/srs/projections/impl/pj_param.hpp>
#include <boost/geometry/srs/projections/par4.hpp>
#include <boost/geometry/srs/projections/proj4.hpp>


namespace boost { namespace geometry { namespace projections {

namespace detail {


/* SEC_TO_RAD = Pi/180/3600 */
template <typename T>
inline T SEC_TO_RAD() { return 4.84813681109535993589914102357e-6; }

template <typename BGParams, typename T>
inline void pj_datum_add_defn(BGParams const& , std::vector<pvalue<T> >& pvalues)
{
    /* -------------------------------------------------------------------- */
    /*      Is there a datum definition in the parameter list?  If so,      */
    /*      add the defining values to the parameter list.  Note that       */
    /*      this will append the ellipse definition as well as the          */
    /*      towgs84= and related parameters.  It should also be pointed     */
    /*      out that the addition is permanent rather than temporary        */
    /*      like most other keyword expansion so that the ellipse           */
    /*      definition will last into the pj_ell_set() function called      */
    /*      after this one.                                                 */
    /* -------------------------------------------------------------------- */
    std::string name = pj_get_param_s(pvalues, "datum");
    if(! name.empty())
    {
        /* find the datum definition */
        const int n = sizeof(pj_datums) / sizeof(pj_datums[0]);
        int index = -1;
        for (int i = 0; i < n && index == -1; i++)
        {
            if(pj_datums[i].id == name)
            {
                index = i;
            }
        }

        if (index == -1)
        {
            BOOST_THROW_EXCEPTION( projection_exception(error_unknown_ellp_param) );
        }

        pj_datums_type const& datum = pj_datums[index];

        if(! datum.ellipse_id.empty())
        {
            pvalues.push_back(pj_mkparam<T>("ellps", datum.ellipse_id));
        }

        if(! datum.defn_n.empty() && ! datum.defn_v.empty())
        {
            pvalues.push_back(pj_mkparam<T>(datum.defn_n, datum.defn_v));
        }
    }
}

template <BOOST_GEOMETRY_PROJECTIONS_DETAIL_TYPENAME_PX, typename T>
inline void pj_datum_add_defn(srs::static_proj4<BOOST_GEOMETRY_PROJECTIONS_DETAIL_PX> const& /*bg_params*/,
                              std::vector<pvalue<T> >& pvalues)
{
    typedef srs::static_proj4<BOOST_GEOMETRY_PROJECTIONS_DETAIL_PX> bg_parameters_type;
    typedef typename srs::par4::detail::tuples_find_if
        <
            bg_parameters_type,
            //srs::par4::detail::is_datum
            srs::par4::detail::is_param_t<srs::par4::datum>::pred
        >::type datum_type;
    typedef typename srs::par4::detail::datum_traits
        <
            datum_type
        > datum_traits;

    // is unknown if datum parameter found but traits are not specialized
    static const bool not_set_or_known = boost::is_same<datum_type, void>::value
                                      || ! boost::is_same<typename datum_traits::ellps_type, void>::value;
    BOOST_MPL_ASSERT_MSG((not_set_or_known), UNKNOWN_DATUM, (bg_parameters_type));

    std::string def_n = datum_traits::def_n();
    std::string def_v = datum_traits::def_v();

    if (! def_n.empty() && ! def_v.empty())
    {
        pvalues.push_back(pj_mkparam<T>(def_n, def_v));
    }
}

/************************************************************************/
/*                            pj_datum_set()                            */
/************************************************************************/

template <typename BGParams, typename T>
inline void pj_datum_set(BGParams const& bg_params, std::vector<pvalue<T> >& pvalues, parameters<T>& projdef)
{
    static const T SEC_TO_RAD = detail::SEC_TO_RAD<T>();

    projdef.datum_type = datum_unknown;

    pj_datum_add_defn(bg_params, pvalues);

/* -------------------------------------------------------------------- */
/*      Check for nadgrids parameter.                                   */
/* -------------------------------------------------------------------- */
    std::string nadgrids = pj_get_param_s(pvalues, "nadgrids");
    std::string towgs84 = pj_get_param_s(pvalues, "towgs84");
    if(! nadgrids.empty())
    {
        /* We don't actually save the value separately.  It will continue
           to exist int he param list for use in pj_apply_gridshift.c */

        projdef.datum_type = datum_gridshift;
    }

/* -------------------------------------------------------------------- */
/*      Check for towgs84 parameter.                                    */
/* -------------------------------------------------------------------- */
    else if(! towgs84.empty())
    {
        int parm_count = 0;

        int n = sizeof(projdef.datum_params) / sizeof(projdef.datum_params[0]);

        /* parse out the pvalues */
        std::vector<std::string> parm;
        boost::split(parm, towgs84, boost::is_any_of(" ,"));
        for (std::vector<std::string>::const_iterator it = parm.begin();
            it != parm.end() && parm_count < n;
            ++it)
        {
            projdef.datum_params[parm_count++] = atof(it->c_str());
        }

        if( projdef.datum_params[3] != 0.0
            || projdef.datum_params[4] != 0.0
            || projdef.datum_params[5] != 0.0
            || projdef.datum_params[6] != 0.0 )
        {
            projdef.datum_type = datum_7param;

            /* transform from arc seconds to radians */
            projdef.datum_params[3] *= SEC_TO_RAD;
            projdef.datum_params[4] *= SEC_TO_RAD;
            projdef.datum_params[5] *= SEC_TO_RAD;
            /* transform from parts per million to scaling factor */
            projdef.datum_params[6] =
                (projdef.datum_params[6]/1000000.0) + 1;
        }
        else
        {
            projdef.datum_type = datum_3param;
        }

        /* Note that pj_init() will later switch datum_type to
           PJD_WGS84 if shifts are all zero, and ellipsoid is WGS84 or GRS80 */
    }
}

} // namespace detail
}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_DATUM_SET_HPP
