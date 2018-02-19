// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015-2016 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSAL_SWITCH_DETECTOR_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSAL_SWITCH_DETECTOR_HPP

#include <cstddef>

#include <boost/range.hpp>

#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/copy_segments.hpp>
#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/is_self_turn.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/assert.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// Generic function (is this used somewhere else too?)
inline ring_identifier ring_id_by_seg_id(segment_identifier const& seg_id)
{
    return ring_identifier(seg_id.source_index, seg_id.multi_index, seg_id.ring_index);
}

template
<
    bool Reverse1,
    bool Reverse2,
    overlay_type OverlayType,
    typename Geometry1,
    typename Geometry2,
    typename Turns,
    typename Clusters,
    typename RobustPolicy,
    typename Visitor
>
struct traversal_switch_detector
{
    enum isolation_type
    {
        isolation_unknown = -1,
        isolation_no = 0,
        isolation_yes = 1,
        isolation_multiple = 2
    };

    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type turn_operation_type;
    typedef std::set<signed_size_type> set_type;

    // Per ring, first turns are collected (in turn_indices), and later
    // a region_id is assigned
    struct merged_ring_properties
    {
        signed_size_type region_id;
        set_type turn_indices;

        merged_ring_properties()
            : region_id(-1)
        {}
    };

    struct connection_properties
    {
        std::size_t count;
        // Contains turn-index OR, if clustered, minus-cluster_id
        set_type unique_turn_ids;
        connection_properties()
            : count(0)
        {}
    };

    typedef std::map<signed_size_type, connection_properties> connection_map;

    // Per region, a set of properties is maintained, including its connections
    // to other regions
    struct region_properties
    {
        signed_size_type region_id;
        isolation_type isolated;
        set_type unique_turn_ids;

        // Maps from connected region_id to their properties
        connection_map connected_region_counts;

        region_properties()
            : region_id(-1)
            , isolated(isolation_unknown)
        {}
    };

    // Keeps turn indices per ring
    typedef std::map<ring_identifier, merged_ring_properties > merge_map;
    typedef std::map<signed_size_type, region_properties> region_connection_map;

    typedef set_type::const_iterator set_iterator;

    inline traversal_switch_detector(Geometry1 const& geometry1, Geometry2 const& geometry2,
            Turns& turns, Clusters& clusters,
            RobustPolicy const& robust_policy, Visitor& visitor)
        : m_geometry1(geometry1)
        , m_geometry2(geometry2)
        , m_turns(turns)
        , m_clusters(clusters)
        , m_robust_policy(robust_policy)
        , m_visitor(visitor)
    {
    }

    bool inspect_difference(set_type& turn_id_difference,
            set_type const& turn_ids,
            set_type const& other_turn_ids) const
    {
        // TODO: consider if std::set_difference can be used in the final version
        int const turn_count = turn_ids.size();
        int const other_turn_count = other_turn_ids.size();

        // First quick check on size (TODO: implement multiple-multiple connections)
        if (turn_count - other_turn_count > 1)
        {
            return false;
        }

        // Check if all turns are also present in the connection.
        // The difference is returned
        for (set_iterator it = turn_ids.begin(); it != turn_ids.end(); ++it)
        {
            signed_size_type const& id = *it;
            if (other_turn_ids.count(id) == 0)
            {
                turn_id_difference.insert(id);
            }
        }
        return true;
    }

    bool one_connection_to_another_region(region_properties const& region) const
    {
        // For example:
        // +----------------------+
        // |                   __ |
        // |                  /  \|
        // |                 |    x
        // |                  \__/|
        // |                      |
        // +----------------------+

        if (region.connected_region_counts.size() == 1)
        {
            connection_properties const& cprop = region.connected_region_counts.begin()->second;
            return cprop.count <= 1;
        }
        return region.connected_region_counts.empty();
    }

    // TODO: might be combined with previous
    bool multiple_connections_to_one_region(region_properties const& region) const
    {
        // For example:
        // +----------------------+
        // |                   __ |
        // |                  /  \|
        // |                 |    x
        // |                  \  /|
        // |                  /  \|
        // |                 |    x
        // |                  \__/|
        // |                      |
        // +----------------------+

        if (region.connected_region_counts.size() == 1)
        {
            connection_properties const& cprop = region.connected_region_counts.begin()->second;
            return cprop.count > 1;
        }
        return false;
    }

    bool one_connection_to_multiple_regions(region_properties const& region) const
    {
        // For example:
        // +----------------------+
        // |                   __ | __
        // |                  /  \|/  |
        // |                 |    x   |
        // |                  \__/|\__|
        // |                      |
        // +----------------------+

        bool first = true;
        signed_size_type first_turn_id = 0;
        for (typename connection_map::const_iterator it = region.connected_region_counts.begin();
             it != region.connected_region_counts.end(); ++it)
        {
            connection_properties const& cprop = it->second;

            if (cprop.count != 1)
            {
                return false;
            }
            signed_size_type const unique_turn_id = *cprop.unique_turn_ids.begin();
            if (first)
            {
                first_turn_id = unique_turn_id;
                first = false;
            }
            else if (first_turn_id != unique_turn_id)
            {
                return false;
            }
        }
        return true;
    }

    bool has_only_isolated_children(region_properties const& region) const
    {
        bool first_with_turn = true;
        bool first_with_multiple = true;
        signed_size_type first_turn_id = 0;
        signed_size_type first_multiple_region_id = 0;

        for (typename connection_map::const_iterator it = region.connected_region_counts.begin();
             it != region.connected_region_counts.end(); ++it)
        {
            signed_size_type const region_id = it->first;
            connection_properties const& cprop = it->second;

            typename region_connection_map::const_iterator mit = m_connected_regions.find(region_id);
            if (mit == m_connected_regions.end())
            {
                // Should not occur
                return false;
            }

            region_properties const& connected_region = mit->second;

            bool const multiple = connected_region.isolated == isolation_multiple;

            if (cprop.count != 1)
            {
                if (! multiple)
                {
                    return false;
                }

                // It connects multiple times to an isolated region.
                // This is allowed as long as it happens only once
                if (first_with_multiple)
                {
                    first_multiple_region_id = connected_region.region_id;
                    first_with_multiple = false;
                }
                else if (first_multiple_region_id != connected_region.region_id)
                {
                    return false;
                }

                // Turns in region should be either present in the connection,
                // of form part of the connection with the other region
                set_type diff;
                if (! inspect_difference(diff, region.unique_turn_ids,
                                  connected_region.unique_turn_ids))
                {
                    return false;
                }
                if (diff.size() > 1)
                {
                    // For now:
                    return false;
                }
            }

            if (connected_region.isolated != isolation_yes && ! multiple)
            {
                signed_size_type const unique_turn_id = *cprop.unique_turn_ids.begin();
                if (first_with_turn)
                {
                    first_turn_id = unique_turn_id;
                    first_with_turn = false;
                }
                else if (first_turn_id != unique_turn_id)
                {
                    return false;
                }
            }
        }
        // If there is only one connection (with a 'parent'), and all other
        // connections are itself isolated, it is isolated
        return true;
    }

    void get_isolated_regions()
    {
        typedef typename region_connection_map::iterator it_type;

        // First time: check regions isolated (one connection only),
        // semi-isolated (multiple connections between same region),
        // and complex isolated (connection with multiple rings but all
        // at same point)
        for (it_type it = m_connected_regions.begin();
             it != m_connected_regions.end(); ++it)
        {
            region_properties& properties = it->second;
            if (one_connection_to_another_region(properties))
            {
                properties.isolated = isolation_yes;
            }
            else if (multiple_connections_to_one_region(properties))
            {
                properties.isolated = isolation_multiple;
            }
            else if (one_connection_to_multiple_regions(properties))
            {
                properties.isolated = isolation_yes;
            }
        }

        // Propagate isolation to next level
        // TODO: should be optimized
        std::size_t defensive_check = 0;
        bool changed = true;
        while (changed && defensive_check++ < m_connected_regions.size())
        {
            changed = false;
            for (it_type it = m_connected_regions.begin();
                 it != m_connected_regions.end(); ++it)
            {
                region_properties& properties = it->second;

                if (properties.isolated == isolation_unknown
                        && has_only_isolated_children(properties))
                {
                    properties.isolated = isolation_yes;
                    changed = true;
                }
            }
        }
    }

    void assign_isolation()
    {
        for (std::size_t turn_index = 0; turn_index < m_turns.size(); ++turn_index)
        {
            turn_type& turn = m_turns[turn_index];

            for (int op_index = 0; op_index < 2; op_index++)
            {
                turn_operation_type& op = turn.operations[op_index];
                typename region_connection_map::const_iterator mit = m_connected_regions.find(op.enriched.region_id);
                if (mit != m_connected_regions.end())
                {
                    region_properties const& prop = mit->second;
                    op.enriched.isolated = prop.isolated == isolation_yes;
                }
            }
        }
    }

    void assign_region_ids()
    {
        for (typename merge_map::const_iterator it
             = m_turns_per_ring.begin(); it != m_turns_per_ring.end(); ++it)
        {
            ring_identifier const& ring_id = it->first;
            merged_ring_properties const& properties = it->second;

            for (set_iterator sit = properties.turn_indices.begin();
                 sit != properties.turn_indices.end(); ++sit)
            {
                turn_type& turn = m_turns[*sit];

                for (int i = 0; i < 2; i++)
                {
                    turn_operation_type& op = turn.operations[i];
                    if (ring_id_by_seg_id(op.seg_id) == ring_id)
                    {
                        op.enriched.region_id = properties.region_id;
                    }
                }
            }
        }
    }

    void assign_connected_regions()
    {
        for (std::size_t turn_index = 0; turn_index < m_turns.size(); ++turn_index)
        {
            turn_type const& turn = m_turns[turn_index];

            signed_size_type const unique_turn_id
                    = turn.is_clustered() ? -turn.cluster_id : turn_index;

            turn_operation_type op0 = turn.operations[0];
            turn_operation_type op1 = turn.operations[1];

            signed_size_type const& id0 = op0.enriched.region_id;
            signed_size_type const& id1 = op1.enriched.region_id;

            // Add region (by assigning) and add involved turns
            if (id0 != -1)
            {
                m_connected_regions[id0].region_id = id0;
                m_connected_regions[id0].unique_turn_ids.insert(unique_turn_id);
            }
            if (id1 != -1 && id0 != id1)
            {
                m_connected_regions[id1].region_id = id1;
                m_connected_regions[id1].unique_turn_ids.insert(unique_turn_id);
            }

            if (id0 != id1 && id0 != -1 && id1 != -1)
            {
                // Assign connections
                connection_properties& prop0 = m_connected_regions[id0].connected_region_counts[id1];
                connection_properties& prop1 = m_connected_regions[id1].connected_region_counts[id0];

                // Reference this turn or cluster to later check uniqueness on ring
                if (prop0.unique_turn_ids.count(unique_turn_id) == 0)
                {
                    prop0.count++;
                    prop0.unique_turn_ids.insert(unique_turn_id);
                }
                if (prop1.unique_turn_ids.count(unique_turn_id) == 0)
                {
                    prop1.count++;
                    prop1.unique_turn_ids.insert(unique_turn_id);
                }
            }
        }
    }

    inline bool connects_same_region(turn_type const& turn) const
    {
        if (turn.discarded)
        {
            // Discarded turns don't connect same region (otherwise discarded colocated uu turn
            // could make a connection)
            return false;
        }

        if (! turn.is_clustered())
        {
            // If it is a uu/ii-turn (non clustered), it is never same region
            return ! (turn.both(operation_union) || turn.both(operation_intersection));
        }

        if (operation_from_overlay<OverlayType>::value == operation_union)
        {
            // It is a cluster, check zones
            // (assigned by sort_by_side/handle colocations) of both operations
            return turn.operations[0].enriched.zone
                    == turn.operations[1].enriched.zone;
        }

        // For an intersection, two regions connect if they are not ii
        // (ii-regions are isolated) or, in some cases, not iu (for example
        // when a multi-polygon is inside an interior ring and connecting it)
        return ! (turn.both(operation_intersection)
                  || turn.combination(operation_intersection, operation_union));
    }


    inline signed_size_type get_region_id(turn_operation_type const& op) const
    {
        return op.enriched.region_id;
    }


    void create_region(signed_size_type& new_region_id, ring_identifier const& ring_id,
                merged_ring_properties& properties, signed_size_type region_id = -1)
    {
        if (properties.region_id > 0)
        {
            // Already handled
            return;
        }

        // Assign new id if this is a new region
        if (region_id == -1)
        {
            region_id = new_region_id++;
        }

        // Assign this ring to specified region
        properties.region_id = region_id;

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSAL_SWITCH_DETECTOR)
        std::cout << " ADD " << ring_id << "  TO REGION " << region_id << std::endl;
#endif

        // Find connecting rings, recursively
        for (set_iterator sit = properties.turn_indices.begin();
             sit != properties.turn_indices.end(); ++sit)
        {
            signed_size_type const turn_index = *sit;
            turn_type const& turn = m_turns[turn_index];
            if (! connects_same_region(turn))
            {
                // This is a non clustered uu/ii-turn, or a cluster connecting different 'zones'
                continue;
            }

            // Union: This turn connects two rings (interior connected), create the region
            // Intersection: This turn connects two rings, set same regions for these two rings
            for (int op_index = 0; op_index < 2; op_index++)
            {
                turn_operation_type const& op = turn.operations[op_index];
                ring_identifier connected_ring_id = ring_id_by_seg_id(op.seg_id);
                if (connected_ring_id != ring_id)
                {
                    propagate_region(new_region_id, connected_ring_id, region_id);
                }
            }
        }
    }

    void propagate_region(signed_size_type& new_region_id,
            ring_identifier const& ring_id, signed_size_type region_id)
    {
        typename merge_map::iterator it = m_turns_per_ring.find(ring_id);
        if (it != m_turns_per_ring.end())
        {
            create_region(new_region_id, ring_id, it->second, region_id);
        }
    }


    void iterate()
    {
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSAL_SWITCH_DETECTOR)
        std::cout << "SWITCH BEGIN ITERATION" << std::endl;
#endif

        // Collect turns per ring
        m_turns_per_ring.clear();
        m_connected_regions.clear();

        for (std::size_t turn_index = 0; turn_index < m_turns.size(); ++turn_index)
        {
            turn_type const& turn = m_turns[turn_index];

            if (turn.discarded
                    && operation_from_overlay<OverlayType>::value == operation_intersection)
            {
                // Discarded turn (union currently still needs it to determine regions)
                continue;
            }

            for (int op_index = 0; op_index < 2; op_index++)
            {
                turn_operation_type const& op = turn.operations[op_index];
                m_turns_per_ring[ring_id_by_seg_id(op.seg_id)].turn_indices.insert(turn_index);
            }
        }

        // All rings having turns are in turns/ring map. Process them.
        {
            signed_size_type new_region_id = 1;
            for (typename merge_map::iterator it
                 = m_turns_per_ring.begin(); it != m_turns_per_ring.end(); ++it)
            {
                create_region(new_region_id, it->first, it->second);
            }

            assign_region_ids();
            assign_connected_regions();
            get_isolated_regions();
            assign_isolation();
        }

        // Now that all regions are filled, assign switch_source property
        // Iterate through all clusters
        for (typename Clusters::iterator it = m_clusters.begin(); it != m_clusters.end(); ++it)
        {
            cluster_info& cinfo = it->second;
            if (cinfo.open_count <= 1)
            {
                // Not a touching cluster
                continue;
            }

            // A touching cluster, gather regions
            set_type regions;
            set_type const& ids = cinfo.turn_indices;

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSAL_SWITCH_DETECTOR)
                std::cout << "SWITCH EXAMINE CLUSTER " << it->first << std::endl;
#endif

            for (set_iterator sit = ids.begin(); sit != ids.end(); ++sit)
            {
                signed_size_type turn_index = *sit;
                turn_type const& turn = m_turns[turn_index];
                for (int oi = 0; oi < 2; oi++)
                {
                    signed_size_type const region_id = get_region_id(turn.operations[oi]);
                    regions.insert(region_id);
                }
            }
            // Switch source if this cluster connects the same region
            cinfo.switch_source = regions.size() <= 1;
        }

        // Iterate through all uu/ii turns (non-clustered)
        for (std::size_t turn_index = 0; turn_index < m_turns.size(); ++turn_index)
        {
            turn_type& turn = m_turns[turn_index];

            if (turn.discarded
                    || turn.blocked()
                    || turn.is_clustered()
                    || ! (turn.both(operation_union) || turn.both(operation_intersection)))
            {
                // Skip discarded, blocked, non-uu/ii and clustered turns
                continue;
            }


            signed_size_type const region0 = get_region_id(turn.operations[0]);
            signed_size_type const region1 = get_region_id(turn.operations[1]);

            // Switch sources for same region
            turn.switch_source = region0 == region1;
        }


#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSAL_SWITCH_DETECTOR)
        std::cout << "SWITCH END ITERATION" << std::endl;

        for (std::size_t turn_index = 0; turn_index < m_turns.size(); ++turn_index)
        {
            turn_type const& turn = m_turns[turn_index];

            if ((turn.both(operation_union) || turn.both(operation_intersection))
                 && ! turn.is_clustered())
            {
                std::cout << "UU/II SWITCH RESULT "
                             << turn_index << " -> "
                          << turn.switch_source << std::endl;
            }
        }

        for (typename Clusters::const_iterator it = m_clusters.begin(); it != m_clusters.end(); ++it)
        {
            cluster_info const& cinfo = it->second;
            if (cinfo.open_count > 1)
            {
                std::cout << "CL SWITCH RESULT " << it->first
                             << " -> " << cinfo.switch_source << std::endl;
            }
            else
            {
                std::cout << "CL SWITCH RESULT " << it->first
                          << " is not registered as open" << std::endl;
            }
        }
#endif

    }

private:

    Geometry1 const& m_geometry1;
    Geometry2 const& m_geometry2;
    Turns& m_turns;
    Clusters& m_clusters;
    merge_map m_turns_per_ring;
    region_connection_map m_connected_regions;
    RobustPolicy const& m_robust_policy;
    Visitor& m_visitor;
};

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSAL_SWITCH_DETECTOR_HPP
