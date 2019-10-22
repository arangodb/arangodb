// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_EUCLIDEAN_RING_HPP
#define BOOST_HANA_TEST_LAWS_EUCLIDEAN_RING_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/concept/constant.hpp>
#include <boost/hana/concept/euclidean_ring.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/div.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/value.hpp>
#include <boost/hana/zero.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename D, typename = when<true>>
    struct TestEuclideanRing : TestEuclideanRing<D, laws> {
        using TestEuclideanRing<D, laws>::TestEuclideanRing;
    };

    template <typename D>
    struct TestEuclideanRing<D, laws> {
        template <typename Xs>
        TestEuclideanRing(Xs xs) {
            hana::for_each(xs, [](auto x) {
                static_assert(EuclideanRing<decltype(x)>{}, "");
            });

#ifdef BOOST_HANA_WORKAROUND_MSVC_DECLTYPEAUTO_RETURNTYPE_662735
            zero<D>(); // force adding zero<D>'s member function to pending temploid list
#endif

            foreach2(xs, [](auto a, auto b) {

                // commutativity
                BOOST_HANA_CHECK(hana::equal(
                    hana::mult(a, b),
                    hana::mult(b, a)
                ));

                only_when_(hana::not_equal(b, zero<D>()),
                hana::make_lazy([](auto a, auto b) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::plus(
                            hana::mult(hana::div(a, b), b),
                            hana::mod(a, b)
                        ),
                        a
                    ));

                    BOOST_HANA_CHECK(hana::equal(
                        hana::mod(zero<D>(), b),
                        zero<D>()
                    ));
                })(a, b));

            });
        }
    };

    template <typename C>
    struct TestEuclideanRing<C, when<Constant<C>::value>>
        : TestEuclideanRing<C, laws>
    {
        template <typename Xs>
        TestEuclideanRing(Xs xs) : TestEuclideanRing<C, laws>{xs} {
            foreach2(xs, [](auto x, auto y) {
                only_when_(hana::not_equal(zero<C>(), y),
                hana::make_lazy([](auto x, auto y) {
                    BOOST_HANA_CHECK(hana::equal(
                        hana::div(hana::value(x), hana::value(y)),
                        hana::value(hana::div(x, y))
                    ));

                    BOOST_HANA_CHECK(hana::equal(
                        hana::mod(hana::value(x), hana::value(y)),
                        hana::value(hana::mod(x, y))
                    ));

                })(x, y));
            });
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_EUCLIDEAN_RING_HPP
