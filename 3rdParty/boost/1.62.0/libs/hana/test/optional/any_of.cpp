// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    ct_eq<2> x{};
    ct_eq<3> y{};

    BOOST_HANA_CONSTANT_CHECK(hana::any_of(hana::just(x), hana::equal.to(x)));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(hana::just(x), hana::equal.to(y))));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(hana::nothing, hana::equal.to(x))));
}
