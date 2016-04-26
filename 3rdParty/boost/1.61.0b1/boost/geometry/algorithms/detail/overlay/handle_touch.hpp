// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_TOUCH_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_TOUCH_HPP

#include <cstddef>

#include <boost/range.hpp>

#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/geometries/concepts/check.hpp>
#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{


template <typename Turns, typename Visitor>
class handle_touch_uu
{
private :
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename boost::range_iterator<Turns>::type turn_iterator;
    typedef typename boost::range_iterator<Turns const>::type turn_const_iterator;

    typedef typename boost::range_iterator
        <
            typename turn_type::container_type const
        >::type operation_const_iterator;

public :

    handle_touch_uu(Visitor& visitor)
        : m_visitor(visitor)
    {}

    inline void apply(detail::overlay::operation_type operation, Turns& turns)
    {
        if (! has_uu(turns))
        {
            // Performance - if there is no u/u at all, nothing to be done
            return;
        }

        // Iterate through all u/u points
        int turn_index = 0;
        for (turn_iterator it = boost::begin(turns);
             it != boost::end(turns);
             ++it, ++turn_index)
        {
            turn_type& turn = *it;
            if (! turn.both(operation_union))
            {
                continue;
            }

            m_visitor.print("handle_touch uu:", turns, turn_index);

            bool const traverse = turn_should_be_traversed(turns, turn, turn_index);
            bool const start = traverse
                               && turn_should_be_startable(turns, turn, turn_index);
            m_visitor.print("handle_touch, ready ", turns, turn_index);
//                          << std::boolalpha
//                          << traverse << " " << start

            if (traverse)
            {
                // Indicate the sources should switch here to create
                // separate rings (outer ring / inner ring)
                turn.switch_source = true;
            }
            // TODO: this is often not correct, fix this
            turn.operations[0].enriched.startable = start;
            turn.operations[1].enriched.startable = start;
        }
    }

private :

    // Generic utility to be moved somewhere else
    static inline
    ring_identifier ring_id_from_seg_id(const segment_identifier& seg_id)
    {
        return ring_identifier(seg_id.source_index,
                               seg_id.multi_index,
                               seg_id.ring_index);
    }

    static inline
    ring_identifier ring_id_from_op(const turn_type& turn,
                                    int operation_index)
    {
        return ring_id_from_seg_id(turn.operations[operation_index].seg_id);
    }

    static inline bool in_range(const Turns& turns, signed_size_type index)
    {
        signed_size_type const turns_size =
                static_cast<signed_size_type>(boost::size(turns));
        return index >= 0 && index < turns_size;
    }

    static inline bool has_uu(const Turns& turns)
    {
        for (turn_const_iterator it = boost::begin(turns);
            it != boost::end(turns);
            ++it)
        {
            const turn_type& turn = *it;
            if (turn.both(operation_union))
            {
                return true;
            }
        }
        return false;
    }

    static inline
    bool turn_should_be_startable(const Turns& turns,
                                  const turn_type& uu_turn,
                                  signed_size_type uu_turn_index)
    {
        return turn_startable(turns, uu_turn, 0, uu_turn_index)
            || turn_startable(turns, uu_turn, 1, uu_turn_index);

    }

    static inline
    bool turn_startable(const Turns& turns,
                        const turn_type& uu_turn,
                        std::size_t op_index,
                        signed_size_type original_turn_index,
                        std::size_t iteration = 0)
    {
        if (iteration >= boost::size(turns))
        {
            // Defensive check to avoid infinite recursion
            return false;
        }

        signed_size_type const index
                = uu_turn.operations[op_index].enriched.travels_to_ip_index;
        if (index == original_turn_index)
        {
            // Completely traveled, having u/u only, via this op_index
            return true;
        }

        if (! in_range(turns, index))
        {
            return false;
        }

        const turn_type& new_turn = turns[index];

        if (new_turn.operations[0].enriched.startable)
        {
            // Already selectable - no need to select u/u turn too
            return false;
        }

        // If this u/u turn is traversed normally (without skipping), sources are switched
        return turn_startable(turns, new_turn, 1 - op_index,
                              original_turn_index, iteration + 1);
    }

    inline bool turn_should_be_traversed(const Turns& turns,
                                  const turn_type& uu_turn,
                                  signed_size_type uu_turn_index)
    {
        return turn_should_be_traversed(turns, uu_turn, uu_turn_index, 0)
            || turn_should_be_traversed(turns, uu_turn, uu_turn_index, 1);
    }

    inline bool turn_should_be_traversed(const Turns& turns,
                                  const turn_type& uu_turn,
                                  signed_size_type uu_turn_index,
                                  int uu_operation_index)
    {
        // Suppose this is a u/u turn between P and Q
        // Examine all other turns on P and check if Q can be reached
        // Use one of the operations and check if you can reach the other
        signed_size_type const to_turn_index
            = uu_turn.operations[uu_operation_index].enriched.travels_to_ip_index;
        if (! in_range(turns, to_turn_index))
        {
            return false;
        }

        m_visitor.print("Examine:", turns, to_turn_index);
        ring_identifier const other_ring_id
            = ring_id_from_op(uu_turn, 1 - uu_operation_index);

        bool complete = false;
        return can_reach(complete, turns, turns[to_turn_index], uu_operation_index,
                         other_ring_id, uu_turn_index, to_turn_index);
    }

    inline bool can_reach(bool& complete, const Turns& turns,
                                 const turn_type& turn,
                                 signed_size_type uu_operation_index,
                                 const ring_identifier& target_ring_id,
                                 signed_size_type uu_turn_index,
                                 signed_size_type to_turn_index,
                                 std::size_t iteration = 0)
    {
        if (complete)
        {
            return false;
        }

        if (turn.cluster_id >= 0)
        {
            // Clustered turns are yet not supported
            return false;
        }

        if (iteration >= boost::size(turns))
        {
            m_visitor.print("Too much iterations");
            // Defensive check to avoid infinite recursion
            return false;
        }

        if (uu_operation_index != -1 && turn.both(operation_union))
        {
            // If we end up in a u/u turn, check the way how, for this operation
            m_visitor.print("Via u/u");
            return can_reach_via(complete, turns, uu_operation_index,
                                 turn.operations[uu_operation_index],
                                 target_ring_id,
                                 uu_turn_index, to_turn_index, iteration);
        }
        else
        {
            // Check if specified ring can be reached via one of both operations
            return can_reach_via(complete, turns, 0, turn.operations[0], target_ring_id,
                                 uu_turn_index, to_turn_index, iteration)
                || can_reach_via(complete, turns, 1, turn.operations[1], target_ring_id,
                                 uu_turn_index, to_turn_index, iteration);
        }
    }

    template <typename Operation>
    inline bool can_reach_via(bool& complete, const Turns& turns,
            signed_size_type operation_index,
            const Operation& operation,
            const ring_identifier& target_ring_id,
            signed_size_type uu_turn_index,
            signed_size_type to_turn_index,
            std::size_t iteration = 0)
    {
        if (operation.operation != operation_union
            && operation.operation != operation_continue)
        {
            return false;
        }

        signed_size_type const index = operation.enriched.travels_to_ip_index;
        if (index == to_turn_index)
        {
            m_visitor.print("Dead end at", turns, index);
            // Completely traveled, the target is not found
            return false;
        }
        if (index == uu_turn_index)
        {
            // End up where trial was started
            m_visitor.print("Travel complete at", turns, index);
            complete = true;
            return false;
        }

        if (! in_range(turns, index))
        {
            return false;
        }

        m_visitor.print("Now to", turns, index, operation_index);
        const turn_type& new_turn = turns[index];

        if (new_turn.both(operation_union))
        {
            ring_identifier const ring_id = ring_id_from_op(new_turn, operation_index);
            if (ring_id == target_ring_id)
            {
                m_visitor.print("Found (at u/u)!");
                return true;
            }
        }
        else
        {
            ring_identifier const ring_id1 = ring_id_from_op(new_turn, 0);
            ring_identifier const ring_id2 = ring_id_from_op(new_turn, 1);
            if (ring_id1 == target_ring_id || ring_id2 == target_ring_id)
            {
                m_visitor.print("Found!");
                return true;
            }
        }

        // Recursively check this turn
        return can_reach(complete, turns, new_turn, operation_index, target_ring_id,
                         uu_turn_index, to_turn_index, iteration + 1);
    }

private :
    Visitor m_visitor;
};

template <typename Turns, typename Visitor>
inline void handle_touch(detail::overlay::operation_type operation,
                         Turns& turns, Visitor& visitor)
{
    handle_touch_uu<Turns, Visitor> handler(visitor);
    handler.apply(operation, turns);
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_TOUCH_HPP
