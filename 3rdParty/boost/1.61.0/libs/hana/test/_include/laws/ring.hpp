// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_RING_HPP
#define BOOST_HANA_TEST_LAWS_RING_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/functional/capture.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/concept/monoid.hpp>
#include <boost/hana/concept/ring.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename R, typename = when<true>>
    struct TestRing : TestRing<R, laws> {
        using TestRing<R, laws>::TestRing;
    };

    template <typename R>
    struct TestRing<R, laws> {
        template <typename Xs>
        TestRing(Xs xs) {
            hana::for_each(xs, hana::capture(xs)([](auto xs, auto x) {
                static_assert(Ring<decltype(x)>::value, "");

                foreach2(xs, hana::capture(x)([](auto x, auto y, auto z) {
                    // associativity
                    BOOST_HANA_CHECK(hana::equal(
                        hana::mult(x, hana::mult(y, z)),
                        hana::mult(hana::mult(x, y), z)
                    ));

                    // distributivity
                    BOOST_HANA_CHECK(hana::equal(
                        hana::mult(x, hana::plus(y, z)),
                        hana::plus(hana::mult(x, y), hana::mult(x, z))
                    ));
                }));

                // right identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::mult(x, one<R>()), x
                ));

                // left identity
                BOOST_HANA_CHECK(hana::equal(
                    hana::mult(one<R>(), x), x
                ));

                // power
                BOOST_HANA_CHECK(hana::equal(
                    hana::power(x, int_c<0>),
                    one<R>()
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::power(x, int_c<1>),
                    x
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::power(x, int_c<2>),
                    hana::mult(x, x)
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::power(x, int_c<3>),
                    hana::mult(hana::mult(x, x), x)
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::power(x, int_c<4>),
                    hana::mult(hana::mult(hana::mult(x, x), x), x)
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::power(x, int_c<5>),
                    hana::mult(hana::mult(hana::mult(hana::mult(x, x), x), x), x)
                ));

            }));
        }
    };

    template <typename C>
    struct TestRing<C, when<Constant<C>::value>>
        : TestRing<C, laws>
    {
        template <typename Xs>
        TestRing(Xs xs) : TestRing<C, laws>{xs} {
            BOOST_HANA_CHECK(hana::equal(
                hana::value(one<C>()),
                one<typename C::value_type>()
            ));

            foreach2(xs, [](auto x, auto y) {
                BOOST_HANA_CHECK(hana::equal(
                    hana::mult(hana::value(x), hana::value(y)),
                    hana::value(hana::mult(x, y))
                ));
            });
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_RING_HPP
