// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/product.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    // first, second
    {
        hana::pair<ct_eq<1>, ct_eq<2>> p;
        BOOST_HANA_CONSTANT_CHECK(hana::equal(hana::first(p), ct_eq<1>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(hana::second(p), ct_eq<2>{}));
    }

    hana::test::TestProduct<hana::pair_tag>{hana::make_tuple(
        ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}
    )};
}
