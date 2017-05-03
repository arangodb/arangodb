// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_LAWS_HASHABLE_HPP
#define BOOST_HANA_TEST_LAWS_HASHABLE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/concept/hashable.hpp>
#include <boost/hana/core/default.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/if.hpp>

#include <laws/base.hpp>


namespace boost { namespace hana { namespace test {
    template <typename G, typename = when<true>>
    struct TestHashable : TestHashable<G, laws> {
        using TestHashable<G, laws>::TestHashable;
    };

    template <typename H>
    struct TestHashable<H, laws> {
        template <typename Xs>
        TestHashable(Xs xs) {
            hana::for_each(xs, [&](auto const& x) {
                hana::for_each(xs, [&](auto const& y) {
                    using X = hana::tag_of_t<decltype(x)>;
                    using Y = hana::tag_of_t<decltype(y)>;
                    constexpr bool comparable = !hana::is_default<
                        hana::equal_impl<X, Y>
                    >::value;

                    hana::if_(hana::bool_c<comparable>,
                        [](auto const& x, auto const& y) {
                            BOOST_HANA_CHECK(
                                hana::equal(x, y)
                                    ^implies^
                                hana::equal(hana::hash(x), hana::hash(y))
                            );
                        },
                        [](auto...) { }
                    )(x, y);
                });
            });
        }
    };
}}} // end namespace boost::hana::test

#endif // !BOOST_HANA_TEST_LAWS_HASHABLE_HPP
