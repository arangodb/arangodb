// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/all_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/functional/curry.hpp>
#include <boost/hana/permutations.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTEXPR_LAMBDA auto is_permutation_of = hana::curry<2>([](auto xs, auto perm) {
    return hana::contains(hana::permutations(xs), perm);
});

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(
        hana::all_of(
            hana::make_tuple(
                hana::make_tuple('1', 2, 3.0),
                hana::make_tuple('1', 3.0, 2),
                hana::make_tuple(2, '1', 3.0),
                hana::make_tuple(2, 3.0, '1'),
                hana::make_tuple(3.0, '1', 2),
                hana::make_tuple(3.0, 2, '1')
            ),
            is_permutation_of(hana::make_tuple('1', 2, 3.0))
        )
    );
}
