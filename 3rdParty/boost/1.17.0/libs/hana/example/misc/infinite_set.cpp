// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/and.hpp>
#include <boost/hana/any_of.hpp>
#include <boost/hana/flatten.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/functional/partial.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/equal.hpp>
#include <boost/hana/fwd/find_if.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/fwd/union.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/is_subset.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/transform.hpp>
namespace hana = boost::hana;


// A `Monad` for searching infinite sets in finite time.
//
// Taken from http://goo.gl/XJeDy8.
struct infinite_set_tag { };

template <typename Find>
struct infinite_set {
    Find find;
    using hana_tag = infinite_set_tag;
};

template <typename Pred>
constexpr infinite_set<Pred> make_infinite_set(Pred pred) {
    return {pred};
}

template <typename X>
constexpr auto singleton(X x) {
    return make_infinite_set([=](auto /*p*/) { return x; });
}

template <typename X, typename Y>
constexpr auto doubleton(X x, Y y) {
    return make_infinite_set([=](auto p) {
        return hana::if_(p(x), x, y);
    });
}

namespace boost { namespace hana {
    template <>
    struct union_impl<infinite_set_tag> {
        template <typename Xs, typename Ys>
        static constexpr auto apply(Xs xs, Ys ys) {
            return flatten(doubleton(xs, ys));
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct equal_impl<infinite_set_tag, infinite_set_tag> {
        template <typename Xs, typename Ys>
        static constexpr auto apply(Xs xs, Ys ys)
        { return and_(is_subset(xs, ys), is_subset(ys, xs)); }
    };


    //////////////////////////////////////////////////////////////////////////
    // Functor
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct transform_impl<infinite_set_tag> {
        template <typename Set, typename F>
        static constexpr auto apply(Set set, F f) {
            return make_infinite_set([=](auto q) {
                return f(set.find(compose(q, f)));
            });
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Applicative
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct lift_impl<infinite_set_tag> {
        template <typename X>
        static constexpr auto apply(X x)
        { return singleton(x); }
    };

    template <>
    struct ap_impl<infinite_set_tag> {
        template <typename F, typename Set>
        static constexpr auto apply(F fset, Set set) {
            return flatten(transform(fset, partial(transform, set)));
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Monad
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct flatten_impl<infinite_set_tag> {
        template <typename Set>
        static constexpr auto apply(Set set) {
            return make_infinite_set([=](auto p) {
                return set.find([=](auto set) {
                    return any_of(set, p);
                }).find(p);
            });
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Searchable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct find_if_impl<infinite_set_tag> {
        template <typename Set, typename Pred>
        static constexpr auto apply(Set set, Pred p) {
            auto x = set.find(p);
            return if_(p(x), hana::just(x), hana::nothing);
        }
    };

    template <>
    struct any_of_impl<infinite_set_tag> {
        template <typename Set, typename Pred>
        static constexpr auto apply(Set set, Pred p) {
            return p(set.find(p));
        }
    };
}} // end namespace boost::hana

//////////////////////////////////////////////////////////////////////////////
// Tests
//////////////////////////////////////////////////////////////////////////////

#include <boost/hana/any_of.hpp>
#include <boost/hana/ap.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/flatten.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_subset.hpp>
#include <boost/hana/lift.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/union.hpp>
namespace hana = boost::hana;


template <int i>
constexpr int n = i;

template <int i>
constexpr auto c = hana::int_c<i>;

int main() {
    auto f = [](auto n) { return n + hana::int_c<10>; };
    auto g = [](auto n) { return n + hana::int_c<100>; };

    // union_
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::union_(singleton(c<0>), singleton(c<0>)),
            singleton(c<0>)
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::union_(singleton(c<0>), singleton(c<1>)),
            doubleton(c<0>, c<1>)
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::union_(singleton(c<0>), doubleton(c<0>, c<1>)),
            doubleton(c<0>, c<1>)
        ));
    }

    // Comparable
    {
        // equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(singleton(n<0>), singleton(n<0>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(singleton(n<0>), singleton(n<1>))));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(singleton(n<0>), doubleton(n<0>, n<0>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(singleton(n<0>), doubleton(n<0>, n<1>))));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(singleton(n<0>), doubleton(n<1>, n<1>))));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(doubleton(n<0>, n<1>), doubleton(n<0>, n<1>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(doubleton(n<0>, n<1>), doubleton(n<1>, n<0>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(doubleton(n<0>, n<1>), doubleton(n<0>, n<0>))));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(doubleton(n<0>, n<1>), doubleton(n<3>, n<4>))));
        }
    }

    // Functor
    {
        // transform
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::transform(singleton(n<0>), f),
                singleton(f(n<0>))
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::transform(doubleton(n<0>, n<1>), f),
                doubleton(f(n<0>), f(n<1>))
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::transform(doubleton(n<0>, n<0>), f),
                singleton(f(n<0>))
            ));
        }
    }

    // Applicative
    {
        // ap
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(singleton(f), singleton(c<0>)),
                singleton(f(c<0>))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(singleton(f), doubleton(c<0>, c<1>)),
                doubleton(f(c<0>), f(c<1>))
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(doubleton(f, g), singleton(c<0>)),
                doubleton(f(c<0>), g(c<0>))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::ap(doubleton(f, g), doubleton(c<0>, c<1>)),
                hana::union_(doubleton(f(c<0>), f(c<1>)),
                             doubleton(g(c<0>), g(c<1>)))
            ));
        }

        // lift
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::lift<infinite_set_tag>(c<0>),
                singleton(c<0>)
            ));
        }
    }

    // Monad
    {
        // flatten
        {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(singleton(singleton(c<0>))),
                singleton(c<0>)
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(singleton(doubleton(c<0>, c<1>))),
                doubleton(c<0>, c<1>)
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(doubleton(singleton(c<0>), singleton(c<1>))),
                doubleton(c<0>, c<1>)
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(doubleton(doubleton(c<0>, c<1>), singleton(c<2>))),
                hana::union_(doubleton(c<0>, c<1>), singleton(c<2>))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(doubleton(singleton(c<0>), doubleton(c<1>, c<2>))),
                hana::union_(doubleton(c<0>, c<1>), singleton(c<2>))
            ));
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(doubleton(doubleton(c<0>, c<1>), doubleton(c<2>, c<3>))),
                hana::union_(doubleton(c<0>, c<1>), doubleton(c<2>, c<3>))
            ));
        }
    }

    // Searchable
    {
        // any_of
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::any_of(singleton(n<0>), hana::equal.to(n<0>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::any_of(singleton(n<0>), hana::equal.to(n<1>))));
            BOOST_HANA_CONSTEXPR_CHECK(hana::any_of(doubleton(n<0>, n<1>), hana::equal.to(n<0>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::any_of(doubleton(n<0>, n<1>), hana::equal.to(n<1>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::any_of(doubleton(n<0>, n<1>), hana::equal.to(n<2>))));
        }

        // find_if
        {
            BOOST_HANA_CONSTANT_CHECK(hana::find_if(singleton(c<0>), hana::equal.to(c<0>)) == hana::just(c<0>));
            BOOST_HANA_CONSTANT_CHECK(hana::find_if(singleton(c<1>), hana::equal.to(c<0>)) == hana::nothing);

            BOOST_HANA_CONSTANT_CHECK(hana::find_if(doubleton(c<0>, c<1>), hana::equal.to(c<0>)) == hana::just(c<0>));
            BOOST_HANA_CONSTANT_CHECK(hana::find_if(doubleton(c<0>, c<1>), hana::equal.to(c<1>)) == hana::just(c<1>));
            BOOST_HANA_CONSTANT_CHECK(hana::find_if(doubleton(c<0>, c<1>), hana::equal.to(c<2>)) == hana::nothing);
        }

        // is_subset
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::is_subset(singleton(n<0>), singleton(n<0>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::is_subset(singleton(n<1>), singleton(n<0>))));

            BOOST_HANA_CONSTEXPR_CHECK(hana::is_subset(singleton(n<0>), doubleton(n<0>, n<1>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::is_subset(singleton(n<1>), doubleton(n<0>, n<1>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::is_subset(singleton(n<2>), doubleton(n<0>, n<1>))));

            BOOST_HANA_CONSTEXPR_CHECK(hana::is_subset(doubleton(n<0>, n<1>), doubleton(n<0>, n<1>)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::is_subset(doubleton(n<0>, n<2>), doubleton(n<0>, n<1>))));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::is_subset(doubleton(n<2>, n<3>), doubleton(n<0>, n<1>))));
        }
    }
}
