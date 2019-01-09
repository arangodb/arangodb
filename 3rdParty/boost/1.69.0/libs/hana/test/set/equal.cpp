// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/not_equal.hpp> // for operator !=
#include <boost/hana/permutations.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto check = [](auto ...keys) {
        auto keys_set = hana::make_set(keys...);
        auto keys_tuple = hana::make_tuple(keys...);
        auto possible_arrangements = hana::permutations(keys_tuple);

        hana::for_each(possible_arrangements, [&](auto perm) {
            BOOST_HANA_CONSTANT_CHECK(hana::equal(
                hana::to_set(perm),
                keys_set
            ));
        });

        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
            keys_set,
            hana::make_set(keys..., ct_eq<999>{})
        )));
    };

    check();
    check(ct_eq<0>{});
    check(ct_eq<0>{}, ct_eq<1>{});
    check(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});

    // check operators
    BOOST_HANA_CONSTANT_CHECK(
        hana::make_set(ct_eq<0>{})
            ==
        hana::make_set(ct_eq<0>{})
    );

    BOOST_HANA_CONSTANT_CHECK(
        hana::make_set(ct_eq<0>{})
            !=
        hana::make_set(ct_eq<1>{})
    );
}
