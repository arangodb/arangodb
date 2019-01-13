// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/pair.hpp>

#include <laws/base.hpp>

#include <utility>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::make<hana::ext::std::pair_tag>(ct_eq<0>{}, ct_eq<1>{}),
        std::make_pair(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::make<hana::ext::std::pair_tag>(ct_eq<3>{}, ct_eq<4>{}),
        std::make_pair(ct_eq<3>{}, ct_eq<4>{})
    ));
}
