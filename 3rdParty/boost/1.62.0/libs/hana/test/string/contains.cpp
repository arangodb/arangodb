// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


struct invalid { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        BOOST_HANA_STRING("abcd"),
        hana::char_c<'a'>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        BOOST_HANA_STRING("abcd"),
        hana::char_c<'c'>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        BOOST_HANA_STRING("abcd"),
        hana::char_c<'e'>
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        BOOST_HANA_STRING("abcd"),
        invalid{}
    )));
}
