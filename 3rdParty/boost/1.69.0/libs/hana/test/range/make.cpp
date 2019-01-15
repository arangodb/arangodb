// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/range.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;


int main() {
    // make sure make<range_tag> works with arbitrary Constants
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::make<hana::range_tag>(hana::test::_constant<1>{}, hana::test::_constant<4>{}),
        hana::make_range(hana::integral_c<int, 1>, hana::integral_c<int, 4>)
    ));
}
