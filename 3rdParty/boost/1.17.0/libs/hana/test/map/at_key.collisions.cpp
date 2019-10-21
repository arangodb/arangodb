// Copyright Jason Rice 2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


struct A { };
struct B { };

struct the_hash;

namespace boost { namespace hana {
    template <>
    struct hash_impl<A> {
        static constexpr auto apply(A const&) {
            return hana::type_c<the_hash>;
        }
    };

    template <>
    struct hash_impl<B> {
        static constexpr auto apply(B const&) {
            return hana::type_c<the_hash>;
        }
    };

    template <>
    struct equal_impl<A, A> {
        static constexpr auto apply(A const&, A const&) {
            return hana::true_c;
        }
    };

    template <>
    struct equal_impl<B, B> {
        static constexpr auto apply(B const&, B const&) {
            return hana::true_c;
        }
    };
}}

int main() {
    constexpr auto key1 = A{};
    constexpr auto key2 = B{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(key1, key1));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(key2, key2));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(key1, key2)));

    // ensure the hashes actually collide
    BOOST_HANA_CONSTANT_CHECK(hana::equal(hana::hash(key1), hana::hash(key2)));

    {
        auto map = hana::to_map(hana::make_tuple(
            hana::make_pair(key1, key1),
            hana::make_pair(key2, key2)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key1),
            key1
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key2),
            key2
        ));
    }

    {
        auto map = hana::to_map(hana::make_tuple(
            hana::make_pair(key2, key2),
            hana::make_pair(key1, key1)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key1),
            key1
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key2),
            key2
        ));
    }

    {
        auto map = hana::to_map(hana::make_tuple(
            hana::make_pair(key1, key1),
            hana::make_pair(hana::int_c<56>, hana::int_c<56>),
            hana::make_pair(key2, key2)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key1),
            key1
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, hana::int_c<56>),
            hana::int_c<56>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key2),
            key2
        ));
    }

    {
        auto map = hana::to_map(hana::make_tuple(
            hana::make_pair(key1, key1),
            hana::make_pair(hana::int_c<56>, hana::int_c<56>),
            hana::make_pair(key2, key2),
            hana::make_pair(hana::int_c<42>, hana::int_c<42>)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key1),
            key1
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, hana::int_c<56>),
            hana::int_c<56>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, key2),
            key2
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at_key(map, hana::int_c<42>),
            hana::int_c<42>
        ));
    }
}
