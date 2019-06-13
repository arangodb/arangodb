// Boost.Geometry
// This file is manually converted from PROJ4

// This file was modified by Oracle on 2018.
// Modifications copyright (c) 2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This file is converted from PROJ4, http://trac.osgeo.org/proj
// PROJ4 is originally written by Gerald Evenden (then of the USGS)
// PROJ4 is maintained by Frank Warmerdam
// This file was converted to Geometry Library by Adam Wulkiewicz

// Original copyright notice:
// Author:   Frank Warmerdam, warmerdam@pobox.com

// Copyright (c) 2000, Frank Warmerdam

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

#ifndef BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_GRIDLIST_SHARED_HPP
#define BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_GRIDLIST_SHARED_HPP


#include <boost/geometry/srs/projections/shared_grids.hpp>
#include <boost/geometry/srs/projections/impl/pj_gridinfo.hpp>


namespace boost { namespace geometry { namespace projections
{

namespace detail
{


/************************************************************************/
/*                       pj_gridlist_merge_grid()                       */
/*                                                                      */
/*      Find/load the named gridfile and merge it into the              */
/*      last_nadgrids_list.                                             */
/************************************************************************/

// Generic stream policy and shared grids
template <typename StreamPolicy>
inline bool pj_gridlist_merge_gridfile(std::string const& gridname,
                                       StreamPolicy const& stream_policy,
                                       srs::shared_grids & grids,
                                       std::vector<std::size_t> & gridindexes)
{
    // Try to find in the existing list of loaded grids.  Add all
    // matching grids as with NTv2 we can get many grids from one
    // file (one shared gridname).    
    {
        boost::shared_lock<boost::shared_mutex> lock(grids.mutex);

        if (pj_gridlist_find_all(gridname, grids.gridinfo, gridindexes))
            return true;
    }

    // Try to load the named grid.
    typename StreamPolicy::stream_type is;
    stream_policy.open(is, gridname);

    pj_gridinfo new_grids;
    
    if (! pj_gridinfo_init(gridname, is, new_grids))
    {
        return false;
    }

    // Add the grid now that it is loaded.

    std::size_t orig_size = 0;
    std::size_t new_size = 0;

    {
        boost::unique_lock<boost::shared_mutex> lock(grids.mutex);

        // Try to find in the existing list of loaded grids again
        // in case other thread already added it.
        if (pj_gridlist_find_all(gridname, grids.gridinfo, gridindexes))
            return true;

        orig_size = grids.gridinfo.size();
        new_size = orig_size + new_grids.size();

        grids.gridinfo.resize(new_size);
        for (std::size_t i = 0 ; i < new_grids.size() ; ++ i)
            new_grids[i].swap(grids.gridinfo[i + orig_size]);
    }
    
    pj_gridlist_add_seq_inc(gridindexes, orig_size, new_size);
    
    return true;
}


} // namespace detail

}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_GRIDLIST_SHARED_HPP
