// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;


struct undefined { };

int main() {
    hana::test::_injection<0> f{};
    hana::test::ct_eq<2> x{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::maybe(x, undefined{}, hana::nothing),
        x
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::maybe(undefined{}, f, hana::just(x)),
        f(x)
    ));
}
