// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_MONOID_HPP
#define BOOST_HANA_TEST_LAWS_MONOID_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/concept/monoid.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename M, typename = when<true>>
    struct TestMonoid : TestMonoid<M, laws> {
        using TestMonoid<M, laws>::TestMonoid;
    };

    template <typename M>
    struct TestMonoid<M, laws> {
        template <typename Xs>
        TestMonoid(Xs xs) {
#ifdef BOOST_HANA_WORKAROUND_MSVC_DECLTYPEAUTO_RETURNTYPE_662735
            zero<M>(); // force adding zero<M>'s member function to pending temploid list
#endif

            hana::for_each(xs, hana::capture(xs)([](auto xs, auto a) {
                static_assert(Monoid<decltype(a)>{}, "");

                // left identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::plus(zero<M>(), a),
                    a
                ));

                // right identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::plus(a, zero<M>()),
                    a
                ));

                hana::for_each(xs,
                hana::capture(xs, a)([](auto xs, auto a, auto b) {
                    hana::for_each(xs,
                    hana::capture(a, b)([](auto a, auto b, auto c) {
                        // associativity
                        BOOST_HANA_CHECK(equal(
                            hana::plus(a, hana::plus(b, c)),
                            hana::plus(hana::plus(a, b), c)
                        ));
                    }));
                }));

            }));
        }
    };

    template <typename C>
    struct TestMonoid<C, when<Constant<C>::value>>
        : TestMonoid<C, laws>
    {
        template <typename Xs>
        TestMonoid(Xs xs) : TestMonoid<C, laws>{xs} {

            BOOST_HANA_CHECK(hana::equal(
                hana::value(zero<C>()),
                zero<typename C::value_type>()
            ));

            foreach2(xs, [](auto x, auto y) {
                BOOST_HANA_CHECK(hana::equal(
                    hana::plus(hana::value(x), hana::value(y)),
                    hana::value(hana::plus(x, y))
                ));
            });
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_MONOID_HPP
