// Boost.Geometry

// This file was modified by Oracle on 2018.
// Modifications copyright (c) 2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_SRS_PROJECTIONS_SHARED_GRIDS_HPP
#define BOOST_GEOMETRY_SRS_PROJECTIONS_SHARED_GRIDS_HPP


#include <boost/geometry/srs/projections/impl/pj_gridinfo.hpp>

#include <boost/thread.hpp>

#include <vector>


namespace boost { namespace geometry
{
    

namespace projections { namespace detail
{

// Forward declaration for functions declarations below
class shared_grids;

// Forward declaratios of shared_grids friends
template <typename StreamPolicy>
inline bool pj_gridlist_merge_gridfile(std::string const& gridname,
                                       StreamPolicy const& stream_policy,
                                       shared_grids & grids,
                                       std::vector<std::size_t> & gridindexes);
template <bool Inverse, typename CalcT, typename StreamPolicy, typename Range>
inline bool pj_apply_gridshift_3(StreamPolicy const& stream_policy,
                                 Range & range,
                                 shared_grids & grids,
                                 std::vector<std::size_t> const& gridindexes);


class shared_grids
{
public:
    std::size_t size() const
    {
        boost::shared_lock<boost::shared_mutex> lock(mutex);
        return gridinfo.size();
    }

    bool empty() const
    {
        boost::shared_lock<boost::shared_mutex> lock(mutex);
        return gridinfo.empty();
    }

private:
    template <typename StreamPolicy>
    friend inline bool projections::detail::pj_gridlist_merge_gridfile(
                            std::string const& gridname,
                            StreamPolicy const& stream_policy,
                            shared_grids & grids,
                            std::vector<std::size_t> & gridindexes);
    template <bool Inverse, typename CalcT, typename StreamPolicy, typename Range>
    friend inline bool projections::detail::pj_apply_gridshift_3(
                            StreamPolicy const& stream_policy,
                            Range & range,
                            shared_grids & grids,
                            std::vector<std::size_t> const& gridindexes);

    projections::detail::pj_gridinfo gridinfo;
    mutable boost::shared_mutex mutex;
};

}} // namespace projections::detail


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_SRS_PROJECTIONS_SHARED_GRIDS_HPP
