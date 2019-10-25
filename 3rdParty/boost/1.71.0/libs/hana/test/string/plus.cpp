// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::plus(BOOST_HANA_STRING(""), BOOST_HANA_STRING("")),
        BOOST_HANA_STRING("")
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::plus(BOOST_HANA_STRING("abcd"), BOOST_HANA_STRING("")),
        BOOST_HANA_STRING("abcd")
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::plus(BOOST_HANA_STRING(""), BOOST_HANA_STRING("abcd")),
        BOOST_HANA_STRING("abcd")
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::plus(BOOST_HANA_STRING("abcd"), BOOST_HANA_STRING("efg")),
        BOOST_HANA_STRING("abcdefg")
    ));

    // check operator
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        BOOST_HANA_STRING("abc") + BOOST_HANA_STRING("def"),
        BOOST_HANA_STRING("abcdef")
    ));
}
