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

#ifndef BOOST_GEOMETRY_PROJECTIONS_PJ_PARAM_HPP
#define BOOST_GEOMETRY_PROJECTIONS_PJ_PARAM_HPP


#include <string>
#include <vector>

#include <boost/geometry/srs/projections/exception.hpp>

#include <boost/geometry/srs/projections/impl/dms_parser.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_integral.hpp>


namespace boost { namespace geometry { namespace projections {

namespace detail {


/* create pvalue list entry */
template <typename T>
inline pvalue<T> pj_mkparam(std::string const& name, std::string const& value)
{
    pvalue<T> newitem;
    newitem.param = name;
    newitem.s = value;
    //newitem.used = false;
    return newitem;
}

/* create pvalue list entry */
template <typename T>
inline pvalue<T> pj_mkparam(std::string const& str)
{
    std::string name = str;
    std::string value;
    boost::trim_left_if(name, boost::is_any_of("+"));
    std::string::size_type loc = name.find("=");
    if (loc != std::string::npos)
    {
        value = name.substr(loc + 1);
        name.erase(loc);
    }

    return pj_mkparam<T>(name, value);
}

/* input exists */
template <typename T>
inline typename std::vector<pvalue<T> >::const_iterator
    pj_param_find(std::vector<pvalue<T> > const& pl, std::string const& name)
{
    typedef typename std::vector<pvalue<T> >::const_iterator iterator;
    for (iterator it = pl.begin(); it != pl.end(); it++)
    {
        if (it->param == name)
        {
            //it->used = true;
            return it;
        }
        // TODO: needed for pipeline
        /*else if (it->param == "step")
        {
            return pl.end();
        }*/
    }

    return pl.end();
}

/* input exists */
template <typename T>
inline bool pj_param_exists(std::vector<pvalue<T> > const& pl, std::string const& name)
{
    return pj_param_find(pl, name) != pl.end();
}

/* integer input */
template <typename T>
inline bool pj_param_i(std::vector<pvalue<T> > const& pl, std::string const& name, int & par)
{
    typename std::vector<pvalue<T> >::const_iterator it = pj_param_find(pl, name);        
    if (it != pl.end())
    {
        par = geometry::str_cast<int>(it->s);
        return true;
    }    
    return false;
}

/* floating point input */
template <typename T>
inline bool pj_param_f(std::vector<pvalue<T> > const& pl, std::string const& name, T & par)
{
    typename std::vector<pvalue<T> >::const_iterator it = pj_param_find(pl, name);        
    if (it != pl.end())
    {
        par = geometry::str_cast<T>(it->s);
        return true;
    }    
    return false;
}

/* radians input */
template <typename T>
inline bool pj_param_r(std::vector<pvalue<T> > const& pl, std::string const& name, T & par)
{
    typename std::vector<pvalue<T> >::const_iterator it = pj_param_find(pl, name);        
    if (it != pl.end())
    {
        dms_parser<T, true> parser;
        par = parser.apply(it->s.c_str()).angle();
        return true;
    }    
    return false;
}

/* string input */
template <typename T>
inline bool pj_param_s(std::vector<pvalue<T> > const& pl, std::string const& name, std::string & par)
{
    typename std::vector<pvalue<T> >::const_iterator it = pj_param_find(pl, name);        
    if (it != pl.end())
    {
        par = it->s;
        return true;
    }    
    return false;
}

/* bool input */
template <typename T>
inline bool pj_get_param_b(std::vector<pvalue<T> > const& pl, std::string const& name)
{
    typename std::vector<pvalue<T> >::const_iterator it = pj_param_find(pl, name);        
    if (it != pl.end())
    {
        switch (it->s[0])
        {
        case '\0': case 'T': case 't':
            return true;
        case 'F': case 'f':
            return false;
        default:
            BOOST_THROW_EXCEPTION( projection_exception(error_invalid_boolean_param) );
            return false;
        }
    }    
    return false;
}

// NOTE: In the original code, in pl_ell_set.c there is a function pj_get_param
// which behavior is similar to pj_param but it doesn't set `user` member to TRUE
// while pj_param does in the original code. In Boost.Geometry this member is not used.
template <typename T>
inline int pj_get_param_i(std::vector<pvalue<T> > const& pl, std::string const& name)
{
    int res = 0;
    pj_param_i(pl, name, res);
    return res;
}

template <typename T>
inline T pj_get_param_f(std::vector<pvalue<T> > const& pl, std::string const& name)
{
    T res = 0;
    pj_param_f(pl, name, res);
    return res;
}

template <typename T>
inline T pj_get_param_r(std::vector<pvalue<T> > const& pl, std::string const& name)
{
    T res = 0;
    pj_param_r(pl, name, res);
    return res;
}

template <typename T>
inline std::string pj_get_param_s(std::vector<pvalue<T> > const& pl, std::string const& name)
{
    std::string res;
    pj_param_s(pl, name, res);
    return res;
}

} // namespace detail
}}} // namespace boost::geometry::projections

#endif
