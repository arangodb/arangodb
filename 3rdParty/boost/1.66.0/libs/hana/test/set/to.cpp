// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/permutations.hpp>
#include <boost/hana/set.hpp>

#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto sequence = ::seq;
    auto foldable = ::seq;
    using S = ::Seq;

    // Set -> Sequence
    {
        auto check = [=](auto ...xs) {
            BOOST_HANA_CONSTANT_CHECK(hana::contains(
                hana::permutations(sequence(xs...)),
                hana::to<S>(hana::make_set(xs...))
            ));
        };
        check();
        check(ct_eq<1>{});
        check(ct_eq<1>{}, ct_eq<2>{});
        check(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{});
        check(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{});
    }

    // Foldable -> Set
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable()),
            hana::make_set()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable(ct_eq<1>{})),
            hana::make_set(ct_eq<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable(ct_eq<1>{}, ct_eq<1>{})),
            hana::make_set(ct_eq<1>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable(ct_eq<1>{}, ct_eq<2>{})),
            hana::make_set(ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable(ct_eq<1>{}, ct_eq<2>{}, ct_eq<1>{})),
            hana::make_set(ct_eq<1>{}, ct_eq<2>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable(ct_eq<1>{}, ct_eq<2>{}, ct_eq<2>{})),
            hana::make_set(ct_eq<1>{}, ct_eq<2>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})),
            hana::make_set(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to<hana::set_tag>(foldable(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{})),
            hana::make_set(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
    }

    // to_set == to<set_tag>
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::to_set(foldable(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{})),
            hana::to<hana::set_tag>(foldable(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}))
        ));
    }
}
