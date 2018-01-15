// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_GROUP_HPP
#define BOOST_HANA_TEST_LAWS_GROUP_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/concept/group.hpp>
#include <boost/hana/lazy.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename G, typename = when<true>>
    struct TestGroup : TestGroup<G, laws> {
        using TestGroup<G, laws>::TestGroup;
    };

    template <typename G>
    struct TestGroup<G, laws> {
        template <typename Xs>
        TestGroup(Xs xs) {
            hana::for_each(xs, [](auto x) {
                static_assert(Group<decltype(x)>{}, "");
            });

            foreach2(xs, [](auto x, auto y) {

                // left inverse
                BOOST_HANA_CHECK(hana::equal(
                    hana::plus(x, hana::negate(x)),
                    zero<G>()
                ));

                // right inverse
                BOOST_HANA_CHECK(hana::equal(
                    hana::plus(hana::negate(x), x),
                    zero<G>()
                ));

                // default definition of minus
                BOOST_HANA_CHECK(hana::equal(
                    hana::minus(x, y),
                    hana::plus(x, hana::negate(y))
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::minus(y, x),
                    hana::plus(y, hana::negate(x))
                ));

                // default definition of negate
                BOOST_HANA_CHECK(hana::equal(
                    hana::negate(hana::negate(x)),
                    x
                ));
            });
        }
    };

    template <typename C>
    struct TestGroup<C, when<Constant<C>::value>>
        : TestGroup<C, laws>
    {
        template <typename Xs>
        TestGroup(Xs xs) : TestGroup<C, laws>{xs} {
            foreach2(xs, [](auto x, auto y) {

                BOOST_HANA_CHECK(hana::equal(
                    hana::negate(hana::value(x)),
                    hana::value(hana::negate(x))
                ));

                BOOST_HANA_CHECK(hana::equal(
                    hana::minus(hana::value(x), hana::value(y)),
                    hana::value(hana::minus(x, y))
                ));

            });
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_GROUP_HPP
