// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/permutations.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    hana::test::_injection<0> f{};

    auto check = [=](auto ...xs) {
        auto arg_perms = hana::permutations(hana::make_tuple(xs...));
        auto possible_results = hana::transform(arg_perms, [=](auto args) {
            return hana::unpack(args, f);
        });

        BOOST_HANA_CONSTANT_CHECK(hana::contains(
            possible_results,
            hana::unpack(hana::make_set(xs...), f)
        ));
    };

    check();
    check(ct_eq<1>{});
    check(ct_eq<1>{}, ct_eq<2>{});
    check(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{});
    check(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{});
}
