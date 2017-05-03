// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/product.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(
        hana::product<>(hana::make_range(hana::int_c<1>, hana::int_c<6>)) == hana::int_c<1 * 2 * 3 * 4 * 5>
    );

    BOOST_HANA_CONSTEXPR_CHECK(
        hana::product<>(hana::make_tuple(1, hana::int_c<3>, hana::long_c<-5>, 9)) == 1 * 3 * -5 * 9
    );

    BOOST_HANA_CONSTEXPR_CHECK(
        hana::product<unsigned long>(hana::make_tuple(2ul, 3ul)) == 6ul
    );
}
