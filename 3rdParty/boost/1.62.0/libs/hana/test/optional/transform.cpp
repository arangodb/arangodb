// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/transform.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct undefined { };

int main() {
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::nothing, undefined{}),
        hana::nothing
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::just(ct_eq<3>{}), f),
        hana::just(f(ct_eq<3>{}))
    ));
}
