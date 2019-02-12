// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/and.hpp>
#include <boost/hana/ap.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/concat.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/flatten.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/lift.hpp>
#include <boost/hana/sum.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


struct tree_tag;

template <typename X, typename Subforest>
struct node {
    X value;
    Subforest subforest;

    using hana_tag = tree_tag;
};

constexpr auto make_forest = hana::make_tuple;

template <typename X, typename Subforest>
constexpr auto make_node(X x, Subforest subforest) {
    return node<X, Subforest>{x, subforest};
}

namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct equal_impl<tree_tag, tree_tag> {
        template <typename Node1, typename Node2>
        static constexpr auto apply(Node1 node1, Node2 node2) {
            return hana::and_(
                hana::equal(node1.value, node2.value),
                hana::equal(node1.subforest, node2.subforest)
            );
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Functor
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct transform_impl<tree_tag> {
        template <typename Node, typename F>
        static constexpr auto apply(Node node, F f) {
            return make_node(
                f(node.value),
                hana::transform(node.subforest, [=](auto subtree) {
                    return hana::transform(subtree, f);
                })
            );
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Applicative
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct lift_impl<tree_tag> {
        template <typename X>
        static constexpr auto apply(X x)
        { return make_node(x, make_forest()); }
    };

    template <>
    struct ap_impl<tree_tag> {
        template <typename F, typename X>
        static constexpr auto apply(F f, X x) {
            return make_node(
                f.value(x.value),
                hana::concat(
                    hana::transform(x.subforest, [=](auto subtree) {
                        return hana::transform(subtree, f.value);
                    }),
                    hana::transform(f.subforest, [=](auto subtree) {
                        return hana::ap(subtree, x);
                    })
                )
            );
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Monad
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct flatten_impl<tree_tag> {
        template <typename Node>
        static constexpr auto apply(Node node) {
            return make_node(
                node.value.value,
                hana::concat(
                    node.value.subforest,
                    hana::transform(node.subforest, hana::flatten)
                )
            );
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct fold_left_impl<tree_tag> {
        template <typename Node, typename State, typename F>
        static constexpr auto apply(Node node, State state, F f) {
            return hana::fold_left(node.subforest, f(state, node.value),
                        [=](auto state, auto subtree) {
                            return hana::fold_left(subtree, state, f);
                        });
        }
    };
}}

int main() {
    constexpr auto tree = make_node(1, make_forest(
        make_node(2, make_forest()),
        make_node(3, make_forest()),
        make_node(4, make_forest())
    ));

    BOOST_HANA_CONSTEXPR_CHECK(hana::sum<>(tree) == 10);

    BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
        hana::transform(tree, [](int i) { return i + 1; }),
        make_node(2, make_forest(
            make_node(3, make_forest()),
            make_node(4, make_forest()),
            make_node(5, make_forest())
        ))
    ));
}
