// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_SELF_TURNS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_SELF_TURNS_HPP

#include <boost/range.hpp>

#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/is_self_turn.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/within.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

struct discard_turns
{
    template <typename Turns, typename Clusters, typename Geometry0, typename Geometry1>
    static inline
    void apply(Turns& , Clusters const& , Geometry0 const& , Geometry1 const& )
    {}
};

template <overlay_type OverlayType, operation_type OperationType>
struct discard_closed_turns : discard_turns {};

// It is only implemented for operation_union, not in buffer
template <>
struct discard_closed_turns<overlay_union, operation_union>
{

    template <typename Turns, typename Clusters, typename Geometry0, typename Geometry1>
    static inline
    void apply(Turns& turns, Clusters const& clusters,
            Geometry0 const& geometry0, Geometry1 const& geometry1)
    {
        typedef typename boost::range_value<Turns>::type turn_type;

        for (typename boost::range_iterator<Turns>::type
                it = boost::begin(turns);
             it != boost::end(turns);
             ++it)
        {
            turn_type& turn = *it;

            if (turn.discarded || ! is_self_turn<overlay_union>(turn))
            {
                continue;
            }

            bool const within =
                    turn.operations[0].seg_id.source_index == 0
                    ? geometry::within(turn.point, geometry1)
                    : geometry::within(turn.point, geometry0);

            if (within)
            {
                // It is in the interior of the other geometry
                turn.discarded = true;
            }
        }
    }
};

struct discard_self_intersection_turns
{
private :

    template <typename Turns, typename Clusters>
    static inline
    bool any_blocked(signed_size_type cluster_id,
            const Turns& turns, Clusters const& clusters)
    {
        typename Clusters::const_iterator cit = clusters.find(cluster_id);
        if (cit == clusters.end())
        {
            return false;
        }
        cluster_info const& cinfo = cit->second;
        for (std::set<signed_size_type>::const_iterator it
             = cinfo.turn_indices.begin();
             it != cinfo.turn_indices.end(); ++it)
        {
            typename boost::range_value<Turns>::type const& turn = turns[*it];
            if (turn.any_blocked())
            {
                return true;
            }
        }
        return false;
    }

    template <typename Turns, typename Clusters>
    static inline
    bool is_self_cluster(signed_size_type cluster_id,
            const Turns& turns, Clusters const& clusters)
    {
        typename Clusters::const_iterator cit = clusters.find(cluster_id);
        if (cit == clusters.end())
        {
            return false;
        }

        cluster_info const& cinfo = cit->second;
        for (std::set<signed_size_type>::const_iterator it
             = cinfo.turn_indices.begin();
             it != cinfo.turn_indices.end(); ++it)
        {
            if (! is_self_turn<overlay_intersection>(turns[*it]))
            {
                return false;
            }
        }

        return true;
    }

    template <typename Turn, typename Geometry0, typename Geometry1>
    static inline
    bool within(Turn const& turn, Geometry0 const& geometry0,
                Geometry1 const& geometry1)
    {
        return turn.operations[0].seg_id.source_index == 0
            ? geometry::within(turn.point, geometry1)
            : geometry::within(turn.point, geometry0);
    }

    template <typename Turns, typename Clusters,
              typename Geometry0, typename Geometry1>
    static inline
    void discard_clusters(Turns& turns, Clusters const& clusters,
            Geometry0 const& geometry0, Geometry1 const& geometry1)
    {
        for (typename Clusters::const_iterator cit = clusters.begin();
             cit != clusters.end(); ++cit)
        {
            signed_size_type cluster_id = cit->first;

            // If there are only self-turns in the cluster, the cluster should
            // be located within the other geometry, for intersection
            if (is_self_cluster(cluster_id, turns, clusters))
            {
                cluster_info const& cinfo = cit->second;
                if (! within(turns[*cinfo.turn_indices.begin()], geometry0, geometry1))
                {
                    // Discard all turns in cluster
                    for (std::set<signed_size_type>::const_iterator sit = cinfo.turn_indices.begin();
                         sit != cinfo.turn_indices.end(); ++sit)
                    {
                        turns[*sit].discarded = true;
                    }
                }
            }
        }
    }

public :

    template <typename Turns, typename Clusters,
              typename Geometry0, typename Geometry1>
    static inline
    void apply(Turns& turns, Clusters const& clusters,
            Geometry0 const& geometry0, Geometry1 const& geometry1)
    {
        discard_clusters(turns, clusters, geometry0, geometry1);

        typedef typename boost::range_value<Turns>::type turn_type;

        for (typename boost::range_iterator<Turns>::type
                it = boost::begin(turns);
             it != boost::end(turns);
             ++it)
        {
            turn_type& turn = *it;

            if (turn.discarded || ! is_self_turn<overlay_intersection>(turn))
            {
                continue;
            }

            segment_identifier const& id0 = turn.operations[0].seg_id;
            segment_identifier const& id1 = turn.operations[1].seg_id;
            if (id0.multi_index != id1.multi_index
                    || (id0.ring_index == -1 && id1.ring_index == -1)
                    || (id0.ring_index >= 0 && id1.ring_index >= 0))
            {
                // Not an ii ring (int/ext) on same ring
                continue;
            }

            if (turn.is_clustered() && turn.has_colocated_both)
            {
                // Don't delete a self-ii-turn colocated with another ii-turn
                // (for example #case_recursive_boxes_70)
                // But for some cases (#case_58_iet) they should be deleted,
                // there are many self-turns there and also blocked turns there
                if (! any_blocked(turn.cluster_id, turns, clusters))
                {
                    continue;
                }
            }

            // It is a ii self-turn
            // Check if it is within the other geometry
            // If not, it can be ignored
            if (! within(turn, geometry0, geometry1))
            {
                // It is not within another geometry, discard the turn
                turn.discarded = true;
            }
        }
    }
};

template <overlay_type OverlayType, operation_type OperationType>
struct discard_open_turns : discard_turns {};

// Handler it for intersection
template <>
struct discard_open_turns<overlay_intersection, operation_intersection>
        : discard_self_intersection_turns {};

// For difference, it should be done in a different way (TODO)


template <overlay_type OverlayType, typename Turns>
inline void discard_self_turns_which_loop(Turns& turns)
{
    if (operation_from_overlay<OverlayType>::value == operation_union)
    {
        // For union, self-turn i/u traveling to itself are allowed to form
        // holes. #case_recursive_boxes_37
        // TODO: this can be finetuned by inspecting the cluster too,
        // and if there are non-self-turns the polygons on their sides can
        // be checked
        return;
    }

    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type op_type;

    signed_size_type turn_index = 0;
    for (typename boost::range_iterator<Turns>::type
            it = boost::begin(turns);
         it != boost::end(turns);
         ++it, ++turn_index)
    {
        turn_type& turn = *it;

        if (! is_self_turn<OverlayType>(turn))
        {
            continue;
        }
        if (! turn.combination(operation_intersection, operation_union))
        {
            // ii may travel to itself
            continue;
        }

        for (int i = 0; i < 2; i++)
        {
            op_type& op = turn.operations[i];

            if (op.enriched.startable
                && op.operation == operation_intersection
                && op.enriched.get_next_turn_index() == turn_index)
            {
                // Self-turn i/u, i part traveling to itself. Discard it.
                // (alternatively it might be made unstartable - but the
                // intersection-operation may not be traveled anyway, and the
                // union-operation is not traveled at all in intersections
                // #case_recursive_boxes_77
                turn.discarded = true;
            }
        }
    }

}

}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_SELF_TURNS_HPP
