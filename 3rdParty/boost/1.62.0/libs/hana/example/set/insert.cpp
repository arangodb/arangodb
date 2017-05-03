// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/insert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto xs = hana::make_set(hana::int_c<0>, hana::type_c<int>);
    BOOST_HANA_CONSTANT_CHECK(
        hana::insert(xs, BOOST_HANA_STRING("abc")) ==
        hana::make_set(hana::int_c<0>, hana::type_c<int>, BOOST_HANA_STRING("abc"))
    );

    BOOST_HANA_CONSTANT_CHECK(
        hana::insert(xs, hana::int_c<0>) == hana::make_set(hana::int_c<0>, hana::type_c<int>)
    );
}
