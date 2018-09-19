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

#ifndef BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_DATUMS_HPP
#define BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_DATUMS_HPP

#include <boost/geometry/srs/projections/impl/projects.hpp>

#include <string>

namespace boost { namespace geometry { namespace projections {

namespace detail {

// Originally defined in projects.h
struct pj_datums_type
{
    std::string id;         /* datum keyword */
    std::string defn_n;     /* e.g. "to_wgs84" */
    std::string defn_v;     /* e.g. "0,0,0" */
    std::string ellipse_id; /* ie from ellipse table */
    std::string comments;   /* EPSG code, etc */
};

// Originally defined in projects.h
struct pj_prime_meridians_type
{
    std::string id;   /* prime meridian keyword */
    std::string defn; /* offset from greenwich in DMS format. */
};

/*
 * The ellipse code must match one from pj_ellps.c.  The datum id should
 * be kept to 12 characters or less if possible.  Use the official OGC
 * datum name for the comments if available.
 */

static const pj_datums_type pj_datums[] =
{
    {"WGS84",         "towgs84",   "0,0,0",
                      "WGS84",     ""},

    {"GGRS87",        "towgs84",   "-199.87,74.79,246.62",
                      "GRS80",     "Greek_Geodetic_Reference_System_1987"},

    {"NAD83",         "towgs84",   "0,0,0",
                      "GRS80",     "North_American_Datum_1983"},

    {"NAD27",         "nadgrids",  "@conus,@alaska,@ntv2_0.gsb,@ntv1_can.dat",
                      "clrk66",    "North_American_Datum_1927"},

    {"potsdam",       "towgs84",   "598.1,73.7,418.2,0.202,0.045,-2.455,6.7",
                      "bessel",    "Potsdam Rauenberg 1950 DHDN"},

    {"carthage",      "towgs84",   "-263.0,6.0,431.0",
                      "clrk80ign", "Carthage 1934 Tunisia"},

    {"hermannskogel", "towgs84",   "577.326,90.129,463.919,5.137,1.474,5.297,2.4232",
                      "bessel",    "Hermannskogel"},

    {"ire65",         "towgs84",   "482.530,-130.596,564.557,-1.042,-0.214,-0.631,8.15",
                      "mod_airy",  "Ireland 1965"},

    {"nzgd49",        "towgs84",   "59.47,-5.04,187.44,0.47,-0.1,1.024,-4.5993",
                      "intl",      "New Zealand Geodetic Datum 1949"},

    {"OSGB36",        "towgs84",   "446.448,-125.157,542.060,0.1502,0.2470,0.8421,-20.4894",
                      "airy",      "Airy 1830"}
};


static const pj_prime_meridians_type pj_prime_meridians[] =
{
    /* id          definition */
    /* --          ---------- */
    { "greenwich", "0dE" },
    { "lisbon",    "9d07'54.862\"W" },
    { "paris",     "2d20'14.025\"E" },
    { "bogota",    "74d04'51.3\"W" },
    { "madrid",    "3d41'16.58\"W" },
    { "rome",      "12d27'8.4\"E" },
    { "bern",      "7d26'22.5\"E" },
    { "jakarta",   "106d48'27.79\"E" },
    { "ferro",     "17d40'W" },
    { "brussels",  "4d22'4.71\"E" },
    { "stockholm", "18d3'29.8\"E" },
    { "athens",    "23d42'58.815\"E" },
    { "oslo",      "10d43'22.5\"E" }
};

} // namespace detail
}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_PROJECTIONS_IMPL_PJ_DATUMS_HPP
