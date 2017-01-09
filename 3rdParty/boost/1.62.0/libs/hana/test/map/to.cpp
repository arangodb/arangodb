// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/permutations.hpp>

#include <laws/base.hpp>
#include <support/minimal_product.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;


template <int i>
auto key() { return hana::test::ct_eq<i>{}; }

template <int i>
auto val() { return hana::test::ct_eq<-i>{}; }

template <int i, int j>
auto p() { return ::minimal_product(key<i>(), val<j>()); }

int main() {
    constexpr auto foldable = ::seq;
    constexpr auto sequence = ::seq;

    // Foldable -> Map
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable()),
            hana::make_map()
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>())),
            hana::make_map(p<1, 1>())
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>(), p<2, 2>())),
            hana::make_map(p<1, 1>(), p<2, 2>())
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>(), p<2, 2>(), p<3, 3>())),
            hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>())
        ));

        // with duplicates
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>(), p<1, 99>())),
            hana::make_map(p<1, 1>())
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>(), p<2, 2>(), p<1, 99>())),
            hana::make_map(p<1, 1>(), p<2, 2>())
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>(), p<2, 2>(), p<1, 99>(), p<2, 99>())),
            hana::make_map(p<1, 1>(), p<2, 2>())
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>(), p<2, 2>(), p<1, 99>(), p<2, 99>(), p<3, 3>())),
            hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>())
        ));
    }

    // Map -> Sequence
    {
        auto check = [=](auto ...xs) {
            BOOST_HANA_CONSTANT_CHECK(hana::contains(
                hana::permutations(sequence(xs...)),
                hana::to<::Seq>(hana::make_map(xs...))
            ));
        };

        check();
        check(p<1, 1>());
        check(p<1, 1>(), p<2, 2>());
        check(p<1, 1>(), p<2, 2>(), p<3, 3>());
        check(p<1, 1>(), p<2, 2>(), p<3, 3>(), p<4, 4>());
    }

    // to_map == to<map_tag>
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_map(foldable(p<1, 1>(), p<2, 2>(), p<1, 99>(), p<2, 99>(), p<3, 3>())),
            hana::to<hana::map_tag>(foldable(p<1, 1>(), p<2, 2>(), p<1, 99>(), p<2, 99>(), p<3, 3>()))
        ));
    }
}
