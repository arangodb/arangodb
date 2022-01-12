// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_MONAD_HPP
#define BOOST_HANA_TEST_LAWS_MONAD_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/chain.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/concept/monad.hpp>
#include <boost/hana/concept/sequence.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/flatten.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/functional/id.hpp>
#include <boost/hana/lift.hpp>
#include <boost/hana/monadic_compose.hpp>
#include <boost/hana/transform.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename M, typename = when<true>>
    struct TestMonad : TestMonad<M, laws> {
        using TestMonad<M, laws>::TestMonad;
    };

    template <typename M>
    struct TestMonad<M, laws> {
        // Xs are Monads over something
        // XXs are Monads over Monads over something
        template <typename Xs, typename XXs>
        TestMonad(Xs xs, XXs xxs) {
            hana::for_each(xs, [](auto m) {
                static_assert(Monad<decltype(m)>{}, "");

                auto f = hana::compose(lift<M>, test::_injection<0>{});
                auto g = hana::compose(lift<M>, test::_injection<1>{});
                auto h = hana::compose(lift<M>, test::_injection<2>{});
                auto x = test::ct_eq<0>{};

                //////////////////////////////////////////////////////////////
                // Laws formulated with `monadic_compose`
                //////////////////////////////////////////////////////////////
                // associativity
                BOOST_HANA_CHECK(hana::equal(
                    hana::monadic_compose(h, hana::monadic_compose(g, f))(x),
                    hana::monadic_compose(hana::monadic_compose(h, g), f)(x)
                ));

                // left identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::monadic_compose(lift<M>, f)(x),
                    f(x)
                ));

                // right identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::monadic_compose(f, lift<M>)(x),
                    f(x)
                ));

                //////////////////////////////////////////////////////////////
                // Laws formulated with `chain`
                //
                // This just provides us with some additional cross-checking,
                // but the documentation does not mention those.
                //////////////////////////////////////////////////////////////
                BOOST_HANA_CHECK(hana::equal(
                    hana::chain(hana::lift<M>(x), f),
                    f(x)
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::chain(m, lift<M>),
                    m
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::chain(m, [f, g](auto x) {
                        return hana::chain(f(x), g);
                    }),
                    hana::chain(hana::chain(m, f), g)
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::transform(m, f),
                    hana::chain(m, hana::compose(lift<M>, f))
                ));

                //////////////////////////////////////////////////////////////
                // Consistency of method definitions
                //////////////////////////////////////////////////////////////
                // consistency of `chain`
                BOOST_HANA_CHECK(hana::equal(
                    hana::chain(m, f),
                    hana::flatten(hana::transform(m, f))
                ));

                // consistency of `monadic_compose`
                BOOST_HANA_CHECK(hana::equal(
                    hana::monadic_compose(f, g)(x),
                    hana::chain(g(x), f)
                ));
            });

            // consistency of `flatten`
            hana::for_each(xxs, [](auto mm) {
                BOOST_HANA_CHECK(hana::equal(
                    hana::flatten(mm),
                    hana::chain(mm, hana::id)
                ));
            });
        }
    };

    template <typename S>
    struct TestMonad<S, when<Sequence<S>::value>>
        : TestMonad<S, laws>
    {
        template <typename Xs, typename XXs>
        TestMonad(Xs xs, XXs xxs)
            : TestMonad<S, laws>{xs, xxs}
        {
            constexpr auto list = make<S>;

            //////////////////////////////////////////////////////////////////
            // flatten
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(list(list(), list())),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(list(list(ct_eq<0>{}), list())),
                list(ct_eq<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(list(list(), list(ct_eq<0>{}))),
                list(ct_eq<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(list(list(ct_eq<0>{}), list(ct_eq<1>{}))),
                list(ct_eq<0>{}, ct_eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::flatten(list(
                    list(ct_eq<0>{}, ct_eq<1>{}),
                    list(),
                    list(ct_eq<2>{}, ct_eq<3>{}),
                    list(ct_eq<4>{})
                )),
                list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
            ));

            // just make sure we don't double move; this happened in hana::tuple
            hana::flatten(list(list(Tracked{1}, Tracked{2})));

            //////////////////////////////////////////////////////////////////
            // chain
            //////////////////////////////////////////////////////////////////
            {
                test::_injection<0> f{};
                auto g = hana::compose(list, f);

                BOOST_HANA_CONSTANT_CHECK(hana::equal(
                    hana::chain(list(), g),
                    list()
                ));

                BOOST_HANA_CONSTANT_CHECK(hana::equal(
                    hana::chain(list(ct_eq<1>{}), g),
                    list(f(ct_eq<1>{}))
                ));

                BOOST_HANA_CONSTANT_CHECK(hana::equal(
                    hana::chain(list(ct_eq<1>{}, ct_eq<2>{}), g),
                    list(f(ct_eq<1>{}), f(ct_eq<2>{}))
                ));

                BOOST_HANA_CONSTANT_CHECK(hana::equal(
                    hana::chain(list(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), g),
                    list(f(ct_eq<1>{}), f(ct_eq<2>{}), f(ct_eq<3>{}))
                ));

                BOOST_HANA_CONSTANT_CHECK(hana::equal(
                    hana::chain(list(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}), g),
                    list(f(ct_eq<1>{}), f(ct_eq<2>{}), f(ct_eq<3>{}), f(ct_eq<4>{}))
                ));
            }

            //////////////////////////////////////////////////////////////////
            // monadic_compose
            //////////////////////////////////////////////////////////////////
            {
                test::_injection<0> f{};
                test::_injection<1> g{};

                auto mf = [=](auto x) { return list(f(x), f(f(x))); };
                auto mg = [=](auto x) { return list(g(x), g(g(x))); };

                auto x = test::ct_eq<0>{};
                BOOST_HANA_CHECK(hana::equal(
                    hana::monadic_compose(mf, mg)(x),
                    list(f(g(x)), f(f(g(x))), f(g(g(x))), f(f(g(g(x)))))
                ));
            }
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_MONAD_HPP
