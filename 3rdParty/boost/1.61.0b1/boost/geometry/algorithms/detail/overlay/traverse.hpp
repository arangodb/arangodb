// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSE_HPP

#include <cstddef>

#include <boost/range.hpp>

#include <boost/geometry/algorithms/detail/overlay/backtrack_check_si.hpp>
#include <boost/geometry/algorithms/detail/overlay/copy_segments.hpp>
#include <boost/geometry/algorithms/detail/overlay/sort_by_side.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/num_points.hpp>
#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>

#if defined(BOOST_GEOMETRY_DEBUG_INTERSECTION) \
    || defined(BOOST_GEOMETRY_OVERLAY_REPORT_WKT) \
    || defined(BOOST_GEOMETRY_DEBUG_TRAVERSE)
#  include <string>
#  include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#  include <boost/geometry/io/wkt/wkt.hpp>
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <typename Turn, typename Operation>
#ifdef BOOST_GEOMETRY_DEBUG_TRAVERSE
inline void debug_traverse(Turn const& turn, Operation op,
                std::string const& header)
{
    std::cout << header
        << " at " << op.seg_id
        << " meth: " << method_char(turn.method)
        << " op: " << operation_char(op.operation)
        << " vis: " << visited_char(op.visited)
        << " of:  " << operation_char(turn.operations[0].operation)
        << operation_char(turn.operations[1].operation)
        << " " << geometry::wkt(turn.point)
        << std::endl;

    if (boost::contains(header, "Finished"))
    {
        std::cout << std::endl;
    }
}
#else
inline void debug_traverse(Turn const& , Operation, const char*)
{
}
#endif


//! Metafunction to define side_order (clockwise, ccw) by operation_type
template <operation_type OpType>
struct side_compare {};

template <>
struct side_compare<operation_union>
{
    typedef std::greater<int> type;
};

template <>
struct side_compare<operation_intersection>
{
    typedef std::less<int> type;
};


template
<
    bool Reverse1,
    bool Reverse2,
    operation_type OperationType,
    typename Geometry1,
    typename Geometry2,
    typename Turns,
    typename Clusters,
    typename RobustPolicy,
    typename Visitor
>
struct traversal
{
    typedef typename side_compare<OperationType>::type side_compare_type;
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type turn_operation_type;

    typedef typename geometry::point_type<Geometry1>::type point_type;
    typedef sort_by_side::side_sorter
        <
            Reverse1, Reverse2,
            point_type, side_compare_type
        > sbs_type;

    inline traversal(Geometry1 const& geometry1, Geometry2 const& geometry2,
            Turns& turns, Clusters const& clusters,
            RobustPolicy const& robust_policy, Visitor& visitor)
        : m_geometry1(geometry1)
        , m_geometry2(geometry2)
        , m_turns(turns)
        , m_clusters(clusters)
        , m_robust_policy(robust_policy)
        , m_visitor(visitor)
    {}


    inline bool select_source(signed_size_type turn_index,
                              segment_identifier const& seg_id1,
                              segment_identifier const& seg_id2)
    {
        if (OperationType == operation_intersection)
        {
            // For intersections always switch sources
            return seg_id1.source_index != seg_id2.source_index;
        }
        else if (OperationType == operation_union)
        {
            // For uu, only switch sources if indicated
            turn_type const& turn = m_turns[turn_index];

            // TODO: pass this information
            bool const is_buffer
                    = turn.operations[0].seg_id.source_index
                      == turn.operations[1].seg_id.source_index;

            if (is_buffer)
            {
                // Buffer does not use source_index (always 0)
                return turn.switch_source
                        ? seg_id1.multi_index != seg_id2.multi_index
                        : seg_id1.multi_index == seg_id2.multi_index;
            }

            return turn.switch_source
                    ? seg_id1.source_index != seg_id2.source_index
                    : seg_id1.source_index == seg_id2.source_index;
        }
        return false;
    }

    inline
    signed_size_type get_next_turn_index(turn_operation_type const& op) const
    {
        return op.enriched.next_ip_index == -1
                ? op.enriched.travels_to_ip_index
                : op.enriched.next_ip_index;
    }

    inline bool traverse_possible(signed_size_type turn_index) const
    {
        if (turn_index == -1)
        {
            return false;
        }

        turn_type const& turn = m_turns[turn_index];

        // It is not a dead end if there is an operation to continue, or of
        // there is a cluster (assuming for now we can get out of the cluster)
        return turn.cluster_id >= 0
            || turn.has(OperationType)
            || turn.has(operation_continue);
    }

    inline bool select_operation(turn_type& turn,
                signed_size_type start_turn_index,
                segment_identifier const& seg_id,
                int& selected_op_index)
    {
        if (turn.discarded)
        {
            return false;
        }

        bool result = false;

        typename turn_operation_type::comparable_distance_type
                max_remaining_distance = 0;

        selected_op_index = -1;
        for (int i = 0; i < 2; i++)
        {
            turn_operation_type const& op = turn.operations[i];
            if (op.visited.started())
            {
                selected_op_index = i;
                return true;
            }

            signed_size_type const next_turn_index = get_next_turn_index(op);

            // In some cases there are two alternatives.
            // For "ii", take the other one (alternate)
            //           UNLESS the other one is already visited
            // For "uu", take the same one (see above);
            // For "cc", take either one, but if there is a starting one,
            //           take that one. If next is dead end, skip that one.
            if (   (op.operation == operation_continue
                    && traverse_possible(next_turn_index)
                    && ! result)
                || (op.operation == OperationType
                    && ! op.visited.finished()
                    && (! result
                        || select_source(next_turn_index, op.seg_id, seg_id)
                        )
                    )
                )
            {
                if (op.operation == operation_continue)
                {
                    max_remaining_distance = op.remaining_distance;
                }
                selected_op_index = i;
                debug_traverse(turn, op, " Candidate");
                result = true;
            }

            if (op.operation == operation_continue && result)
            {
                if (next_turn_index == start_turn_index)
                {
                    selected_op_index = i;
                    debug_traverse(turn, op, " Candidate override (start)");
                }
                else if (op.remaining_distance > max_remaining_distance)
                {
                    max_remaining_distance = op.remaining_distance;
                    selected_op_index = i;
                    debug_traverse(turn, op, " Candidate override (remaining)");
                }
            }
        }

        if (result)
        {
           debug_traverse(turn, turn.operations[selected_op_index], "  Accepted");
        }

        return result;
    }

    inline bool select_from_cluster(signed_size_type& turn_index,
        int& op_index, signed_size_type start_turn_index,
        sbs_type const& sbs, bool allow_pass_rank)
    {
        bool const is_union = OperationType == operation_union;
        bool const is_intersection = OperationType == operation_intersection;

        std::size_t selected_rank = 0;
        bool result = false;
        for (std::size_t i = 0; i < sbs.m_ranked_points.size(); i++)
        {
            typename sbs_type::rp const& ranked_point = sbs.m_ranked_points[i];
            if (result && ranked_point.main_rank > selected_rank)
            {
                return result;
            }

            turn_type const& ranked_turn = m_turns[ranked_point.turn_index];
            turn_operation_type const& ranked_op = ranked_turn.operations[ranked_point.op_index];

            if (result && ranked_op.visited.finalized())
            {
                // One of the arcs in the same direction as the selected result
                // is already traversed.
                return false;
            }

            if (ranked_point.index == sort_by_side::index_to
                && (ranked_point.main_rank > 0
                    || ranked_turn.both(operation_continue)))
            {
                if ((is_union
                     && ranked_op.enriched.count_left == 0
                     && ranked_op.enriched.count_right > 0)
                || (is_intersection
                     && ranked_op.enriched.count_right == 2))
                {
                    if (result && ranked_point.turn_index != start_turn_index)
                    {
                        // Don't override - only override if arrive at start
                        continue;
                    }

                    turn_index = ranked_point.turn_index;
                    op_index = ranked_point.op_index;

                    if (is_intersection
                        && ranked_turn.both(operation_intersection)
                        && ranked_op.visited.finalized())
                    {
                        // Override:
                        // For a ii turn, even though one operation might be selected,
                        // it should take the other one if the first one is used in a completed ring
                        op_index = 1 - ranked_point.op_index;
                    }

                    result = true;
                    selected_rank = ranked_point.main_rank;
                }
                else if (! allow_pass_rank)
                {
                    return result;
                }
            }
        }
        return result;
    }

    inline bool select_turn_from_cluster(signed_size_type& turn_index,
            int& op_index, signed_size_type start_turn_index,
            int start_op_index, bool is_start,
            point_type const& point)
    {
        bool const is_union = OperationType == operation_union;

        turn_type const& turn = m_turns[turn_index];
        BOOST_ASSERT(turn.cluster_id >= 0);

#ifdef BOOST_GEOMETRY_DEBUG_TRAVERSE_BUFFER
        std::cout << "Select Cluster "
                  << turn.cluster_id
                 << " from " << turn_index
                 << "[" << op_index << "]"
                  << std::boolalpha << std::endl;
#endif

        typename Clusters::const_iterator mit = m_clusters.find(turn.cluster_id);
        BOOST_ASSERT(mit != m_clusters.end());

        std::set<signed_size_type> const& ids = mit->second;

        sbs_type sbs;
        sbs.set_origin(point);

        bool at_start = false;
        bool has_finished = false;

        for (typename std::set<signed_size_type>::const_iterator sit = ids.begin();
             sit != ids.end(); ++sit)
        {
            signed_size_type cluster_turn_index = *sit;
            turn_type const& cluster_turn = m_turns[cluster_turn_index];
            if (cluster_turn.discarded)
            {
                // Defensive check, discarded turns should not be in cluster
                continue;
            }

            if (! is_start && ! is_union && cluster_turn_index == start_turn_index)
            {
                at_start = true;
            }

            for (int i = 0; i < 2; i++)
            {
                sbs.add(cluster_turn.operations[i], cluster_turn_index, i,
                        m_geometry1, m_geometry2, false);

                if (cluster_turn.operations[i].visited.finished())
                {
                    has_finished = true;
                }
            }
        }

        if (at_start && has_finished)
        {
            turn_index = start_turn_index;
            op_index = start_op_index;
            return true;
        }

        sbs.apply(turn.point);

        int open_count = 0;
        if (is_union)
        {
            // Check how many open spaces there are.
            // TODO: might be moved to sbs itself, though it also uses turns

            std::size_t last_rank = 0;
            for (std::size_t i = 0; i < sbs.m_ranked_points.size(); i++)
            {
                typename sbs_type::rp const& ranked_point = sbs.m_ranked_points[i];

                if (ranked_point.main_rank > last_rank
                    && ranked_point.index == sort_by_side::index_to)
                {
                    turn_type const& ranked_turn = m_turns[ranked_point.turn_index];
                    turn_operation_type const& ranked_op = ranked_turn.operations[ranked_point.op_index];
                    if (ranked_op.enriched.count_left == 0
                         && ranked_op.enriched.count_right > 0)
                    {
                        open_count++;
                        last_rank = ranked_point.main_rank;
                    }
                }
            }
        }

        if (open_count > 1)
        {
            sbs.reverse();
            return select_from_cluster(turn_index, op_index, start_turn_index, sbs, true);
        }

        return select_from_cluster(turn_index, op_index, start_turn_index, sbs, false);
    }

    inline void change_index_for_self_turn(signed_size_type& to_vertex_index,
                turn_type const& start_turn,
                turn_operation_type const& start_op,
                int start_op_index)
    {
        turn_operation_type const& other_op
                = start_turn.operations[1 - start_op_index];
        if (start_op.seg_id.source_index != other_op.seg_id.source_index)
        {
            // Not a buffer/self-turn
            return;
        }

        // It travels to itself, can happen. If this is a buffer, it can
        // sometimes travel to itself in the following configuration:
        //
        // +---->--+
        // |       |
        // |   +---*----+ *: one turn, with segment index 2/7
        // |   |   |    |
        // |   +---C    | C: closing point (start/end)
        // |            |
        // +------------+
        //
        // If it starts on segment 2 and travels to itself on segment 2, that
        // should be corrected to 7 because that is the shortest path
        //
        // Also a uu turn (touching with another buffered ring) might have this
        // apparent configuration, but there it should
        // always travel the whole ring

        bool const correct
                = ! start_turn.both(operation_union)
                  && start_op.seg_id.segment_index == to_vertex_index;

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE)
        std::cout << " WARNING: self-buffer "
                  << " correct=" << correct
                  << " turn=" << operation_char(start_turn.operations[0].operation)
                  << operation_char(start_turn.operations[1].operation)
                  << " start=" << start_op.seg_id.segment_index
                  << " from=" << to_vertex_index
                  << " to=" << other_op.enriched.travels_to_vertex_index
                  << std::endl;
#endif

        if (correct)
        {
            to_vertex_index = other_op.enriched.travels_to_vertex_index;
        }
    }

    template <typename Ring>
    inline traverse_error_type travel_to_next_turn(signed_size_type start_turn_index,
                int start_op_index,
                signed_size_type& turn_index,
                int& op_index,
                segment_identifier& seg_id,
                Ring& current_ring,
                bool is_start)
    {
        int const previous_op_index = op_index;
        signed_size_type const previous_turn_index = turn_index;
        turn_type& previous_turn = m_turns[turn_index];
        turn_operation_type& previous_op = previous_turn.operations[op_index];

        // If there is no next IP on this segment
        if (previous_op.enriched.next_ip_index < 0)
        {
            if (previous_op.enriched.travels_to_vertex_index < 0
                || previous_op.enriched.travels_to_ip_index < 0)
            {
                return is_start
                        ? traverse_error_no_next_ip_at_start
                        : traverse_error_no_next_ip;
            }

            signed_size_type to_vertex_index = previous_op.enriched.travels_to_vertex_index;

            if (is_start &&
                    previous_op.enriched.travels_to_ip_index == start_turn_index)
            {
                change_index_for_self_turn(to_vertex_index, previous_turn,
                    previous_op, start_op_index);
            }

            if (previous_op.seg_id.source_index == 0)
            {
                geometry::copy_segments<Reverse1>(m_geometry1,
                        previous_op.seg_id, to_vertex_index,
                        m_robust_policy, current_ring);
            }
            else
            {
                geometry::copy_segments<Reverse2>(m_geometry2,
                        previous_op.seg_id, to_vertex_index,
                        m_robust_policy, current_ring);
            }
            seg_id = previous_op.seg_id;
            turn_index = previous_op.enriched.travels_to_ip_index;
        }
        else
        {
            turn_index = previous_op.enriched.next_ip_index;
            seg_id = previous_op.seg_id;
        }

        // turn_index is not yet finally selected, can change for clusters
        bool const has_cluster = m_turns[turn_index].cluster_id >= 0;
        if (has_cluster)
        {

            if (! select_turn_from_cluster(turn_index, op_index,
                    start_turn_index, start_op_index, is_start, current_ring.back()))
            {
                return is_start
                    ? traverse_error_no_next_ip_at_start
                    : traverse_error_no_next_ip;
            }

            if (is_start && turn_index == previous_turn_index)
            {
                op_index = previous_op_index;
            }
        }

        turn_type& current_turn = m_turns[turn_index];
        detail::overlay::append_no_dups_or_spikes(current_ring, current_turn.point,
            m_robust_policy);

        if (is_start)
        {
            // Register the start
            previous_op.visited.set_started();
            m_visitor.visit_traverse(m_turns, previous_turn, previous_op, "Start");
        }

        if (! has_cluster)
        {
            if (! select_operation(current_turn,
                            start_turn_index,
                            seg_id,
                            op_index))
            {
                return is_start
                    ? traverse_error_dead_end_at_start
                    : traverse_error_dead_end;
            }
        }

        turn_operation_type& op = current_turn.operations[op_index];
        if (op.visited.finalized() || op.visited.visited())
        {
            return traverse_error_visit_again;
        }

        // Register the visit
        set_visited(current_turn, op);
        m_visitor.visit_traverse(m_turns, current_turn, op, "Visit");

        return traverse_error_none;
    }

    inline void finalize_visit_info()
    {
        for (typename boost::range_iterator<Turns>::type
            it = boost::begin(m_turns);
            it != boost::end(m_turns);
            ++it)
        {
            turn_type& turn = *it;
            for (int i = 0; i < 2; i++)
            {
                turn_operation_type& op = turn.operations[i];
                op.visited.finalize();
            }
        }
    }

    inline void set_visited(turn_type& turn, turn_operation_type& op)
    {
        // On "continue", set "visited" for ALL directions in this turn
        if (op.operation == detail::overlay::operation_continue)
        {
            for (int i = 0; i < 2; i++)
            {
                turn_operation_type& op = turn.operations[i];
                if (op.visited.none())
                {
                    op.visited.set_visited();
                }
            }
        }
        else
        {
            op.visited.set_visited();
        }
    }


    template <typename Ring>
    inline traverse_error_type traverse(Ring& ring,
            signed_size_type start_turn_index, int start_op_index)
    {
        turn_type const& start_turn = m_turns[start_turn_index];
        turn_operation_type& start_op = m_turns[start_turn_index].operations[start_op_index];

        detail::overlay::append_no_dups_or_spikes(ring, start_turn.point,
            m_robust_policy);

        signed_size_type current_turn_index = start_turn_index;
        int current_op_index = start_op_index;
        segment_identifier current_seg_id;

        traverse_error_type error = travel_to_next_turn(start_turn_index,
                    start_op_index,
                    current_turn_index, current_op_index, current_seg_id,
                    ring, true);

        if (error != traverse_error_none)
        {
            // This is not necessarily a problem, it happens for clustered turns
            // which are "build in" or otherwise point inwards
            return error;
        }

        if (current_turn_index == start_turn_index)
        {
            start_op.visited.set_finished();
            m_visitor.visit_traverse(m_turns, m_turns[current_turn_index], start_op, "Early finish");
            return traverse_error_none;
        }

        std::size_t const max_iterations = 2 + 2 * m_turns.size();
        for (std::size_t i = 0; i <= max_iterations; i++)
        {
            // We assume clockwise polygons only, non self-intersecting, closed.
            // However, the input might be different, and checking validity
            // is up to the library user.

            // Therefore we make here some sanity checks. If the input
            // violates the assumptions, the output polygon will not be correct
            // but the routine will stop and output the current polygon, and
            // will continue with the next one.

            // Below three reasons to stop.
            error = travel_to_next_turn(start_turn_index, start_op_index,
                    current_turn_index, current_op_index, current_seg_id,
                    ring, false);

            if (error != traverse_error_none)
            {
                return error;
            }

            if (current_turn_index == start_turn_index
                    && current_op_index == start_op_index)
            {
                start_op.visited.set_finished();
                m_visitor.visit_traverse(m_turns, start_turn, start_op, "Finish");
                return traverse_error_none;
            }
        }

        return traverse_error_endless_loop;
    }

private :
    Geometry1 const& m_geometry1;
    Geometry2 const& m_geometry2;
    Turns& m_turns;
    Clusters const& m_clusters;
    RobustPolicy const& m_robust_policy;
    Visitor& m_visitor;
};



/*!
    \brief Traverses through intersection points / geometries
    \ingroup overlay
 */
template
<
    bool Reverse1, bool Reverse2,
    typename Geometry1,
    typename Geometry2,
    operation_type OpType,
    typename Backtrack = backtrack_check_self_intersections<Geometry1, Geometry2>
>
class traverse
{
public :
    template
    <
        typename RobustPolicy,
        typename Turns,
        typename Rings,
        typename Visitor,
        typename Clusters
    >
    static inline void apply(Geometry1 const& geometry1,
                Geometry2 const& geometry2,
                RobustPolicy const& robust_policy,
                Turns& turns, Rings& rings,
                Clusters const& clusters,
                Visitor& visitor)
    {
        typedef typename boost::range_value<Rings>::type ring_type;
        typedef typename boost::range_value<Turns>::type turn_type;
        typedef typename turn_type::turn_operation_type op_type;

        traversal
            <
                Reverse1, Reverse2, OpType,
                Geometry1, Geometry2,
                Turns, Clusters,
                RobustPolicy, Visitor
            > trav(geometry1, geometry2, turns, clusters,
                   robust_policy, visitor);

        std::size_t const min_num_points
                = core_detail::closure::minimum_ring_size
                        <
                            geometry::closure<ring_type>::value
                        >::value;

        std::size_t finalized_ring_size = boost::size(rings);

        typename Backtrack::state_type state;

        // Iterate through all unvisited points
        for (std::size_t turn_index = 0; turn_index < turns.size(); ++turn_index)
        {
            turn_type const& start_turn = turns[turn_index];

            // Skip discarded ones
            if (start_turn.discarded
                || start_turn.blocked())
            {
                continue;
            }

            for (int op_index = 0; op_index < 2; op_index++)
            {
                op_type const& start_op = start_turn.operations[op_index];

                if (! start_op.visited.none()
                    || ! start_op.enriched.startable
                    || start_op.visited.rejected()
                    || ! (start_op.operation == OpType
                        || start_op.operation == detail::overlay::operation_continue))
                {
                    continue;
                }

                ring_type ring;
                traverse_error_type traverse_error = trav.traverse(ring,
                                turn_index, op_index);

                if (traverse_error == traverse_error_none)
                {
                    if (geometry::num_points(ring) >= min_num_points)
                    {
                        clean_closing_dups_and_spikes(ring, robust_policy);
                        rings.push_back(ring);

                        trav.finalize_visit_info();
                        finalized_ring_size++;
                    }
                }
                else
                {
                    Backtrack::apply(
                        finalized_ring_size,
                        rings, ring, turns, start_turn,
                        turns[turn_index].operations[op_index],
                        traverse_error,
                        geometry1, geometry2, robust_policy, state, visitor);
                }
            }
        }
    }
};

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_TRAVERSE_HPP
