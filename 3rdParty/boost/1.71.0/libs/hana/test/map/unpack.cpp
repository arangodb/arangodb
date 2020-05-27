// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/unpack.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/permutations.hpp>
#include <boost/hana/transform.hpp>

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

struct undefined { };

int main() {
    auto sequence = ::seq;
    hana::test::_injection<0> f{};

    auto check = [=](auto ...pairs) {
        auto possible_results = hana::transform(hana::permutations(sequence(pairs...)),
            [=](auto xs) { return hana::unpack(xs, f); }
        );

        BOOST_HANA_CONSTANT_CHECK(hana::contains(
            possible_results,
            hana::unpack(hana::make_map(pairs...), f)
        ));
    };

    check();
    check(p<1, 1>());
    check(p<1, 1>(), p<2, 2>());
    check(p<1, 1>(), p<2, 2>(), p<3, 3>());
    check(p<1, 1>(), p<2, 2>(), p<3, 3>(), p<4, 4>());
}
