// Boost.Geometry Index
//
// R-tree nodes storing static-size containers
// test version throwing exceptions on creation
//
// Copyright (c) 2011-2014 Adam Wulkiewicz, Lodz, Poland.
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_INDEX_TEST_RTREE_THROWING_NODE_HPP
#define BOOST_GEOMETRY_INDEX_TEST_RTREE_THROWING_NODE_HPP

#include <rtree/exceptions/test_throwing.hpp>

struct throwing_nodes_stats
{
    static void reset_counters() { get_internal_nodes_counter_ref() = 0; get_leafs_counter_ref() = 0; }
    static size_t internal_nodes_count() { return get_internal_nodes_counter_ref(); }
    static size_t leafs_count() { return get_leafs_counter_ref(); }

    static size_t & get_internal_nodes_counter_ref() { static size_t cc = 0; return cc; }
    static size_t & get_leafs_counter_ref() { static size_t cc = 0; return cc; }
};

namespace boost { namespace geometry { namespace index {

template <size_t MaxElements, size_t MinElements>
struct linear_throwing : public linear<MaxElements, MinElements> {};

template <size_t MaxElements, size_t MinElements>
struct quadratic_throwing : public quadratic<MaxElements, MinElements> {};

template <size_t MaxElements, size_t MinElements, size_t OverlapCostThreshold = 0, size_t ReinsertedElements = detail::default_rstar_reinserted_elements_s<MaxElements>::value>
struct rstar_throwing : public rstar<MaxElements, MinElements, OverlapCostThreshold, ReinsertedElements> {};

namespace detail { namespace rtree {

// options implementation (from options.hpp)

struct node_throwing_static_tag {};

template <size_t MaxElements, size_t MinElements>
struct options_type< linear_throwing<MaxElements, MinElements> >
{
    typedef options<
        linear_throwing<MaxElements, MinElements>,
        insert_default_tag, choose_by_content_diff_tag, split_default_tag, linear_tag,
        node_throwing_static_tag
    > type;
};

template <size_t MaxElements, size_t MinElements>
struct options_type< quadratic_throwing<MaxElements, MinElements> >
{
    typedef options<
        quadratic_throwing<MaxElements, MinElements>,
        insert_default_tag, choose_by_content_diff_tag, split_default_tag, quadratic_tag,
        node_throwing_static_tag
    > type;
};

template <size_t MaxElements, size_t MinElements, size_t OverlapCostThreshold, size_t ReinsertedElements>
struct options_type< rstar_throwing<MaxElements, MinElements, OverlapCostThreshold, ReinsertedElements> >
{
    typedef options<
        rstar_throwing<MaxElements, MinElements, OverlapCostThreshold, ReinsertedElements>,
        insert_reinsert_tag, choose_by_overlap_diff_tag, split_default_tag, rstar_tag,
        node_throwing_static_tag
    > type;
};

}} // namespace detail::rtree

// node implementation

namespace detail { namespace rtree {

template <typename Value, typename Parameters, typename Box, typename Allocators>
struct variant_internal_node<Value, Parameters, Box, Allocators, node_throwing_static_tag>
{
    typedef throwing_varray<
        rtree::ptr_pair<Box, typename Allocators::node_pointer>,
        Parameters::max_elements + 1
    > elements_type;

    template <typename Alloc>
    inline variant_internal_node(Alloc const&) { throwing_nodes_stats::get_internal_nodes_counter_ref()++; }
    inline ~variant_internal_node() { throwing_nodes_stats::get_internal_nodes_counter_ref()--; }

    // required because variants are initialized using node object
    // temporary must be taken into account
    inline variant_internal_node(variant_internal_node const& n)
        : elements(n.elements)
    {
        throwing_nodes_stats::get_internal_nodes_counter_ref()++;
    }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    inline variant_internal_node(variant_internal_node && n)
        : elements(boost::move(n.elements))
    {
        throwing_nodes_stats::get_internal_nodes_counter_ref()++;
    }
#endif

    elements_type elements;

private:
    variant_internal_node & operator=(variant_internal_node const& n);
};

template <typename Value, typename Parameters, typename Box, typename Allocators>
struct variant_leaf<Value, Parameters, Box, Allocators, node_throwing_static_tag>
{
    typedef throwing_varray<Value, Parameters::max_elements + 1> elements_type;

    template <typename Alloc>
    inline variant_leaf(Alloc const&) { throwing_nodes_stats::get_leafs_counter_ref()++; }
    inline ~variant_leaf() { throwing_nodes_stats::get_leafs_counter_ref()--; }

    // required because variants are initialized using node object
    // temporary must be taken into account
    inline variant_leaf(variant_leaf const& n)
        : elements(n.elements)
    {
        throwing_nodes_stats::get_leafs_counter_ref()++;
    }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    inline variant_leaf(variant_leaf && n)
        : elements(boost::move(n.elements))
    {
        throwing_nodes_stats::get_leafs_counter_ref()++;
    }
#endif

    elements_type elements;

private:
    variant_leaf & operator=(variant_leaf const& n);
};

// nodes traits

template <typename Value, typename Parameters, typename Box, typename Allocators>
struct node<Value, Parameters, Box, Allocators, node_throwing_static_tag>
{
    typedef boost::variant<
        variant_leaf<Value, Parameters, Box, Allocators, node_throwing_static_tag>,
        variant_internal_node<Value, Parameters, Box, Allocators, node_throwing_static_tag>
    > type;
};

template <typename Value, typename Parameters, typename Box, typename Allocators>
struct internal_node<Value, Parameters, Box, Allocators, node_throwing_static_tag>
{
    typedef variant_internal_node<Value, Parameters, Box, Allocators, node_throwing_static_tag> type;
};

template <typename Value, typename Parameters, typename Box, typename Allocators>
struct leaf<Value, Parameters, Box, Allocators, node_throwing_static_tag>
{
    typedef variant_leaf<Value, Parameters, Box, Allocators, node_throwing_static_tag> type;
};

// visitor traits

template <typename Value, typename Parameters, typename Box, typename Allocators, bool IsVisitableConst>
struct visitor<Value, Parameters, Box, Allocators, node_throwing_static_tag, IsVisitableConst>
{
    typedef static_visitor<> type;
};

// allocators

template <typename Allocator, typename Value, typename Parameters, typename Box>
class allocators<Allocator, Value, Parameters, Box, node_throwing_static_tag>
    : public Allocator::template rebind<
        typename node<
            Value, Parameters, Box,
            allocators<Allocator, Value, Parameters, Box, node_throwing_static_tag>,
            node_throwing_static_tag
        >::type
    >::other
{
    typedef typename Allocator::template rebind<
        Value
    >::other value_allocator_type;

public:
    typedef Allocator allocator_type;

    typedef Value value_type;
    typedef value_type & reference;
    typedef const value_type & const_reference;
    typedef typename value_allocator_type::size_type size_type;
    typedef typename value_allocator_type::difference_type difference_type;
    typedef typename value_allocator_type::pointer pointer;
    typedef typename value_allocator_type::const_pointer const_pointer;

    typedef typename Allocator::template rebind<
        typename node<Value, Parameters, Box, allocators, node_throwing_static_tag>::type
    >::other::pointer node_pointer;

//    typedef typename Allocator::template rebind<
//        typename internal_node<Value, Parameters, Box, allocators, node_throwing_static_tag>::type
//    >::other::pointer internal_node_pointer;

    typedef typename Allocator::template rebind<
        typename node<Value, Parameters, Box, allocators, node_throwing_static_tag>::type
    >::other node_allocator_type;

    inline allocators()
        : node_allocator_type()
    {}

    template <typename Alloc>
    inline explicit allocators(Alloc const& alloc)
        : node_allocator_type(alloc)
    {}

    inline allocators(BOOST_FWD_REF(allocators) a)
        : node_allocator_type(boost::move(a.node_allocator()))
    {}

    inline allocators & operator=(BOOST_FWD_REF(allocators) a)
    {
        node_allocator() = boost::move(a.node_allocator());
        return *this;
    }

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    inline allocators & operator=(allocators const& a)
    {
        node_allocator() = a.node_allocator();
        return *this;
    }
#endif

    void swap(allocators & a)
    {
        boost::swap(node_allocator(), a.node_allocator());
    }

    bool operator==(allocators const& a) const { return node_allocator() == a.node_allocator(); }
    template <typename Alloc>
    bool operator==(Alloc const& a) const { return node_allocator() == node_allocator_type(a); }

    Allocator allocator() const { return Allocator(node_allocator()); }

    node_allocator_type & node_allocator() { return *this; }
    node_allocator_type const& node_allocator() const { return *this; }
};

struct node_bad_alloc : public std::exception
{
    const char * what() const throw() { return "internal node creation failed."; }
};

struct throwing_node_settings
{
    static void throw_if_required()
    {
        // throw if counter meets max count
        if ( get_max_calls_ref() <= get_calls_counter_ref() )
            throw node_bad_alloc();
        else
            ++get_calls_counter_ref();
    }

    static void reset_calls_counter() { get_calls_counter_ref() = 0; }
    static void set_max_calls(size_t mc) { get_max_calls_ref() = mc; }

    static size_t & get_calls_counter_ref() { static size_t cc = 0; return cc; }
    static size_t & get_max_calls_ref() { static size_t mc = (std::numeric_limits<size_t>::max)(); return mc; }
};

// create_node

template <typename Allocators, typename Value, typename Parameters, typename Box>
struct create_node<
    Allocators,
    variant_internal_node<Value, Parameters, Box, Allocators, node_throwing_static_tag>
>
{
    static inline typename Allocators::node_pointer
    apply(Allocators & allocators)
    {
        // throw if counter meets max count
        throwing_node_settings::throw_if_required();

        return create_variant_node<
            typename Allocators::node_pointer,
            variant_internal_node<Value, Parameters, Box, Allocators, node_throwing_static_tag>
        >::apply(allocators.node_allocator());
    }
};

template <typename Allocators, typename Value, typename Parameters, typename Box>
struct create_node<
    Allocators,
    variant_leaf<Value, Parameters, Box, Allocators, node_throwing_static_tag>
>
{
    static inline typename Allocators::node_pointer
    apply(Allocators & allocators)
    {
        // throw if counter meets max count
        throwing_node_settings::throw_if_required();

        return create_variant_node<
            typename Allocators::node_pointer,
            variant_leaf<Value, Parameters, Box, Allocators, node_throwing_static_tag>
        >::apply(allocators.node_allocator());
    }
};

}} // namespace detail::rtree

}}} // namespace boost::geometry::index

#endif // BOOST_GEOMETRY_INDEX_TEST_RTREE_THROWING_NODE_HPP
