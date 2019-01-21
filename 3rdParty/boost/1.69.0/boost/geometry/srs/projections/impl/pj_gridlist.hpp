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

#ifndef BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_GRIDLIST_HPP
#define BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_GRIDLIST_HPP


#include <boost/geometry/srs/projections/grids.hpp>
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

// Originally one function, here divided into several functions
// with overloads for various types of grids and stream policies

inline bool pj_gridlist_find_all(std::string const& gridname,
                                 pj_gridinfo const& grids,
                                 std::vector<std::size_t> & gridindexes)
{
    bool result = false;
    for (std::size_t i = 0 ; i < grids.size() ; ++i)
    {
        if (grids[i].gridname == gridname)
        {
            result = true;
            gridindexes.push_back(i);
        }
    }
    return result;
}

// Fill container with sequentially increasing numbers
inline void pj_gridlist_add_seq_inc(std::vector<std::size_t> & gridindexes,
                                    std::size_t first, std::size_t last)
{
    gridindexes.reserve(gridindexes.size() + (last - first));
    for ( ; first < last ; ++first)
    {
        gridindexes.push_back(first);
    }
}

// Generic stream policy and standard grids
template <typename StreamPolicy>
inline bool pj_gridlist_merge_gridfile(std::string const& gridname,
                                       StreamPolicy const& stream_policy,
                                       srs::grids & grids,
                                       std::vector<std::size_t> & gridindexes)
{
    // Try to find in the existing list of loaded grids.  Add all
    // matching grids as with NTv2 we can get many grids from one
    // file (one shared gridname).    
    if (pj_gridlist_find_all(gridname, grids.gridinfo, gridindexes))
        return true;

    std::size_t orig_size = grids.gridinfo.size();

    // Try to load the named grid.
    typename StreamPolicy::stream_type is;
    stream_policy.open(is, gridname);

    if (! pj_gridinfo_init(gridname, is, grids.gridinfo))
    {
        return false;
    }

    // Add the grid now that it is loaded.
    pj_gridlist_add_seq_inc(gridindexes, orig_size, grids.gridinfo.size());
    
    return true;
}


/************************************************************************/
/*                     pj_gridlist_from_nadgrids()                      */
/*                                                                      */
/*      This functions loads the list of grids corresponding to a       */
/*      particular nadgrids string into a list, and returns it. The     */
/*      list is kept around till a request is made with a different     */
/*      string in order to cut down on the string parsing cost, and     */
/*      the cost of building the list of tables each time.              */
/************************************************************************/

template <typename StreamPolicy, typename Grids>
inline void pj_gridlist_from_nadgrids(srs::detail::nadgrids const& nadgrids,
                                      StreamPolicy const& stream_policy,
                                      Grids & grids,
                                      std::vector<std::size_t> & gridindexes)

{
    // Loop processing names out of nadgrids one at a time.
    for (srs::detail::nadgrids::const_iterator it = nadgrids.begin() ;
            it != nadgrids.end() ; ++it)
    {
        bool required = (*it)[0] != '@';
        
        std::string name(it->begin() + (required ? 0 : 1), it->end());

        if ( ! pj_gridlist_merge_gridfile(name, stream_policy, grids, gridindexes) 
          && required )
        {
            BOOST_THROW_EXCEPTION( projection_exception(error_failed_to_load_grid) );
        }
    }
}

template <typename Par, typename GridsStorage>
inline void pj_gridlist_from_nadgrids(Par const& defn, srs::projection_grids<GridsStorage> & grids)
{
    BOOST_GEOMETRY_ASSERT(grids.storage_ptr != NULL);

    pj_gridlist_from_nadgrids(defn.nadgrids,
                              grids.storage_ptr->stream_policy,
                              grids.storage_ptr->hgrids,
                              grids.hindexes);
}


} // namespace detail

}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_GRIDLIST_HPP
