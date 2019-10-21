// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct undefined { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::nothing.value_or(ct_eq<3>{}),
        ct_eq<3>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::just(ct_eq<4>{}).value_or(undefined{}),
        ct_eq<4>{}
    ));
}
