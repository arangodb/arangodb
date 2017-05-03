// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fold_right.hpp>
#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;


int main() {
    hana::test::ct_eq<2> x{};
    hana::test::ct_eq<3> s{};
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::fold_right(hana::just(x), s, f),
        f(x, s)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::fold_right(hana::nothing, s, f),
        s
    ));
}
