// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_MONAD_PLUS_HPP
#define BOOST_HANA_TEST_LAWS_MONAD_PLUS_HPP

#include <boost/hana/append.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/chain.hpp>
#include <boost/hana/concat.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/concept/monad_plus.hpp>
#include <boost/hana/concept/sequence.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/cycle.hpp>
#include <boost/hana/empty.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/filter.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/lift.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/prefix.hpp>
#include <boost/hana/prepend.hpp>
#include <boost/hana/remove.hpp>
#include <boost/hana/remove_if.hpp>
#include <boost/hana/replicate.hpp>
#include <boost/hana/suffix.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename M, typename = when<true>>
    struct TestMonadPlus : TestMonadPlus<M, laws> {
        using TestMonadPlus<M, laws>::TestMonadPlus;
    };

    template <typename M>
    struct TestMonadPlus<M, laws> {
        template <typename Xs, typename Predicates, typename Values>
        TestMonadPlus(Xs xs, Predicates predicates, Values values) {
#ifdef BOOST_HANA_WORKAROUND_MSVC_DECLTYPEAUTO_RETURNTYPE_662735
            empty<M>(); // force adding empty<M>'s member function to pending temploid list
#endif

            hana::for_each(xs, [](auto a) {
                static_assert(MonadPlus<decltype(a)>{}, "");

                // left identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::concat(hana::empty<M>(), a),
                    a
                ));

                // right identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::concat(a, hana::empty<M>()),
                    a
                ));

                // absorption
                auto f = hana::compose(lift<M>, test::_injection<0>{});
                BOOST_HANA_CHECK(hana::equal(
                    hana::chain(hana::empty<M>(), f),
                    hana::empty<M>()
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::chain(a, hana::always(hana::empty<M>())),
                    hana::empty<M>()
                ));

            });

            // associativity
            foreach3(xs, [](auto a, auto b, auto c) {
                BOOST_HANA_CHECK(hana::equal(
                    hana::concat(a, hana::concat(b, c)),
                    hana::concat(hana::concat(a, b), c)
                ));
            });

            // Default method definitions
            hana::for_each(xs, hana::capture(predicates, values)(
            [](auto predicates, auto values, auto x) {

                // remove_if(x, pred) == filter(x, negated pred)
                hana::for_each(predicates, hana::capture(x)([](auto x, auto pred) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::remove_if(x, pred),
                        hana::filter(x, hana::compose(hana::not_, pred))
                    ));
                }));

                // remove(x, value) == remove_if(x, equal.to(value))
                hana::for_each(values, hana::capture(x)([](auto x, auto value) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::remove(x, value),
                        hana::remove_if(x, hana::equal.to(value))
                    ));
                }));
            }));
        }
    };

    template <typename S>
    struct TestMonadPlus<S, when<Sequence<S>::value>>
        : TestMonadPlus<S, laws>
    {
        template <int i>
        using eq = test::ct_eq<i>;

        struct undefined { };

        template <typename Xs, typename Predicates, typename Values>
        TestMonadPlus(Xs xs, Predicates predicates, Values values)
            : TestMonadPlus<S, laws>{xs, predicates, values}
        {
            auto z = eq<999>{};
            constexpr auto list = make<S>;

            //////////////////////////////////////////////////////////////////
            // empty
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::empty<S>(), list()
            ));

            //////////////////////////////////////////////////////////////////
            // concat
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(), list()),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(), list(eq<0>{})),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(), list(eq<0>{}, eq<1>{})),
                list(eq<0>{}, eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(eq<0>{}), list()),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(eq<0>{}), list(eq<1>{})),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(eq<0>{}), list(eq<1>{}, eq<2>{})),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(eq<0>{}, eq<1>{}), list()),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(eq<0>{}, eq<1>{}), list(eq<2>{})),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(eq<0>{}, eq<1>{}), list(eq<2>{}, eq<3>{})),
                list(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{})
            ));


            BOOST_HANA_CONSTANT_CHECK(equal(
                concat(list(), list()),
                list()
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                concat(list(1), list()),
                list(1)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                concat(list(), list(1)),
                list(1)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                concat(list(1), list('2')),
                list(1, '2')
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                concat(list(1, '2'), list(3.3)),
                list(1, '2', 3.3)
            ));


            //////////////////////////////////////////////////////////////////
            // filter
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(), undefined{}),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(z), not_equal.to(z)),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(eq<1>{}), not_equal.to(z)),
                list(eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(eq<1>{}, eq<2>{}), not_equal.to(z)),
                list(eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(z, eq<2>{}), not_equal.to(z)),
                list(eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(eq<1>{}, z), not_equal.to(z)),
                list(eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(z, eq<2>{}, eq<3>{}), not_equal.to(z)),
                list(eq<2>{}, eq<3>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(eq<1>{}, z, eq<3>{}), not_equal.to(z)),
                list(eq<1>{}, eq<3>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(eq<1>{}, eq<2>{}, z), not_equal.to(z)),
                list(eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(eq<1>{}, z, z), not_equal.to(z)),
                list(eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(z, eq<2>{}, z), not_equal.to(z)),
                list(eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(z, z, eq<3>{}), not_equal.to(z)),
                list(eq<3>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                filter(list(eq<1>{}, eq<2>{}, eq<3>{}, eq<4>{}, z), not_equal.to(z)),
                list(eq<1>{}, eq<2>{}, eq<3>{}, eq<4>{})
            ));

            //////////////////////////////////////////////////////////////////
            // prepend
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                prepend(list(), eq<0>{}),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                prepend(list(eq<1>{}), eq<0>{}),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                prepend(list(eq<1>{}, eq<2>{}), eq<0>{}),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                prepend(list(eq<1>{}, eq<2>{}, eq<3>{}), eq<0>{}),
                list(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{})
            ));


            BOOST_HANA_CONSTEXPR_CHECK(equal(
                prepend(list(), 1),
                list(1)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                prepend(list('2'), 1),
                list(1, '2')
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                prepend(list('2', 3.3), 1),
                list(1, '2', 3.3)
            ));

            //////////////////////////////////////////////////////////////////
            // append
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                append(list(), eq<0>{}),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                append(list(eq<0>{}), eq<1>{}),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                append(list(eq<0>{}, eq<1>{}), eq<2>{}),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                append(list(), 1), list(1)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                append(list(1), '2'), list(1, '2')
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                append(list(1, '2'), 3.3), list(1, '2', 3.3)
            ));


            //////////////////////////////////////////////////////////////////
            // cycle
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(), size_c<0>),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(), size_c<1>),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(), size_c<2>),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(), size_c<3>),
                list()
            ));


            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}), size_c<0>),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}), size_c<1>),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}), size_c<2>),
                list(eq<0>{}, eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}), size_c<3>),
                list(eq<0>{}, eq<0>{}, eq<0>{})
            ));


            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}), size_c<0>),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}), size_c<1>),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}), size_c<2>),
                list(eq<0>{}, eq<1>{}, eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}), size_c<3>),
                list(eq<0>{}, eq<1>{}, eq<0>{}, eq<1>{}, eq<0>{}, eq<1>{})
            ));


            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}, eq<2>{}), size_c<0>),
                list()
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}, eq<2>{}), size_c<1>),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}, eq<2>{}), size_c<2>),
                list(eq<0>{}, eq<1>{}, eq<2>{}, eq<0>{}, eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                cycle(list(eq<0>{}, eq<1>{}, eq<2>{}), size_c<3>),
                list(eq<0>{}, eq<1>{}, eq<2>{}, eq<0>{}, eq<1>{}, eq<2>{}, eq<0>{}, eq<1>{}, eq<2>{})
            ));

            //////////////////////////////////////////////////////////////////
            // remove_if
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(), undefined{}),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(eq<0>{}), equal.to(z)),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(z), equal.to(z)),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(eq<0>{}, eq<1>{}), equal.to(z)),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(z, eq<1>{}), equal.to(z)),
                list(eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(eq<0>{}, z), equal.to(z)),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(z, z), equal.to(z)),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(eq<0>{}, eq<1>{}, eq<2>{}), equal.to(z)),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(eq<0>{}, eq<1>{}, z), equal.to(z)),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(eq<0>{}, z, eq<2>{}), equal.to(z)),
                list(eq<0>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(z, eq<1>{}, eq<2>{}), equal.to(z)),
                list(eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(z, z, eq<2>{}), equal.to(z)),
                list(eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(eq<0>{}, z, z), equal.to(z)),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                remove_if(list(z, z, z), equal.to(z)),
                list()
            ));

            //////////////////////////////////////////////////////////////////
            // remove
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(), undefined{}),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(eq<0>{}), z),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(z), z),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(eq<0>{}, eq<1>{}), z),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(z, eq<1>{}), z),
                list(eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(eq<0>{}, z), z),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(z, z), z),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(eq<0>{}, eq<1>{}, eq<2>{}), z),
                list(eq<0>{}, eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(eq<0>{}, eq<1>{}, z), z),
                list(eq<0>{}, eq<1>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(eq<0>{}, z, eq<2>{}), z),
                list(eq<0>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(z, eq<1>{}, eq<2>{}), z),
                list(eq<1>{}, eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(z, z, eq<2>{}), z),
                list(eq<2>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(eq<0>{}, z, z), z),
                list(eq<0>{})
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                hana::remove(list(z, z, z), z),
                list()
            ));

            //////////////////////////////////////////////////////////////////
            // replicate
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                replicate<S>(eq<0>{}, size_c<0>),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                replicate<S>(eq<0>{}, size_c<1>),
                list(eq<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                replicate<S>(eq<0>{}, size_c<2>),
                list(eq<0>{}, eq<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                replicate<S>(eq<0>{}, size_c<3>),
                list(eq<0>{}, eq<0>{}, eq<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                replicate<S>(eq<0>{}, size_c<4>),
                list(eq<0>{}, eq<0>{}, eq<0>{}, eq<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                replicate<S>(eq<0>{}, size_c<5>),
                list(eq<0>{}, eq<0>{}, eq<0>{}, eq<0>{}, eq<0>{})
            ));


            //////////////////////////////////////////////////////////////////
            // prefix
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                prefix(list(), z),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                prefix(list(eq<0>{}), z),
                list(z, eq<0>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                prefix(list(eq<0>{}, eq<1>{}), z),
                list(z, eq<0>{}, z, eq<1>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                prefix(list(eq<0>{}, eq<1>{}, eq<2>{}), z),
                list(z, eq<0>{}, z, eq<1>{}, z, eq<2>{})
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                prefix(list(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{}), z),
                list(z, eq<0>{}, z, eq<1>{}, z, eq<2>{}, z, eq<3>{})
            ));


            //////////////////////////////////////////////////////////////////
            // suffix
            //////////////////////////////////////////////////////////////////
            BOOST_HANA_CONSTANT_CHECK(equal(
                suffix(list(), z),
                list()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                suffix(list(eq<0>{}), z),
                list(eq<0>{}, z)
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                suffix(list(eq<0>{}, eq<1>{}), z),
                list(eq<0>{}, z, eq<1>{}, z)
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                suffix(list(eq<0>{}, eq<1>{}, eq<2>{}), z),
                list(eq<0>{}, z, eq<1>{}, z, eq<2>{}, z)
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                suffix(list(eq<0>{}, eq<1>{}, eq<2>{}, eq<3>{}), z),
                list(eq<0>{}, z, eq<1>{}, z, eq<2>{}, z, eq<3>{}, z)
            ));
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_MONAD_PLUS_HPP
