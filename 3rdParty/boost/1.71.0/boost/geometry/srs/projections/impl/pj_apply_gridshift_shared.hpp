// Boost.Geometry
// This file is manually converted from PROJ4

// This file was modified by Oracle on 2018, 2019.
// Modifications copyright (c) 2018-2019, Oracle and/or its affiliates.
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

#ifndef BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_APPLY_GRIDSHIFT_SHARED_HPP
#define BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_APPLY_GRIDSHIFT_SHARED_HPP


#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/radian_access.hpp>

#include <boost/geometry/srs/projections/impl/pj_apply_gridshift.hpp>
#include <boost/geometry/srs/projections/impl/pj_gridlist_shared.hpp>


namespace boost { namespace geometry { namespace projections
{

namespace detail
{

template <bool Inverse, typename CalcT, typename StreamPolicy, typename Range>
inline bool pj_apply_gridshift_3(StreamPolicy const& stream_policy,
                                 Range & range,
                                 shared_grids & grids,
                                 std::vector<std::size_t> const& gridindexes)
{
    typedef typename boost::range_size<Range>::type size_type;
    
    // If the grids are empty the indexes are as well
    if (gridindexes.empty())
    {
        //pj_ctx_set_errno(ctx, PJD_ERR_FAILED_TO_LOAD_GRID);
        //return PJD_ERR_FAILED_TO_LOAD_GRID;
        return false;
    }

    size_type point_count = boost::size(range);

    // local storage
    pj_gi_load local_gi;

    for (size_type i = 0 ; i < point_count ; )
    {
        bool load_needed = false;

        CalcT in_lon = 0;
        CalcT in_lat = 0;

        {
            boost::shared_lock<boost::shared_mutex> lock(grids.mutex);

            for ( ; i < point_count ; ++i )
            {
                typename boost::range_reference<Range>::type
                    point = range::at(range, i);

                in_lon = geometry::get_as_radian<0>(point);
                in_lat = geometry::get_as_radian<1>(point);

                pj_gi * gip = find_grid(in_lon, in_lat, grids.gridinfo, gridindexes);

                if (gip == NULL)
                {
                    // do nothing
                }
                else if (! gip->ct.cvs.empty())
                {
                    // TODO: use set_invalid_point() or similar mechanism
                    CalcT out_lon = HUGE_VAL;
                    CalcT out_lat = HUGE_VAL;

                    nad_cvt<Inverse>(in_lon, in_lat, out_lon, out_lat, *gip);

                    // TODO: check differently
                    if (out_lon != HUGE_VAL)
                    {
                        geometry::set_from_radian<0>(point, out_lon);
                        geometry::set_from_radian<1>(point, out_lat);
                    }
                }
                else
                {
                    // loading is needed
                    local_gi = *gip;
                    load_needed = true;
                    break;
                }
            }
        }

        if (load_needed)
        {
            if (load_grid(stream_policy, local_gi))
            {
                boost::unique_lock<boost::shared_mutex> lock(grids.mutex);

                // check again in case other thread already loaded the grid.
                pj_gi * gip = find_grid(in_lon, in_lat, grids.gridinfo, gridindexes);

                if (gip != NULL && gip->ct.cvs.empty())
                {
                    // swap loaded local storage with empty grid
                    local_gi.swap(*gip);
                }
            }
            else
            {
                ++i;
            }
        }
    }

    return true;
}


} // namespace detail

}}} // namespace boost::geometry::projections

#endif // BOOST_GEOMETRY_SRS_PROJECTIONS_IMPL_PJ_APPLY_GRIDSHIFT_SHARED_HPP
