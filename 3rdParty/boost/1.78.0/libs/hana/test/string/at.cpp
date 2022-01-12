// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        BOOST_HANA_STRING("abcd")[hana::size_c<2>],
        hana::char_c<'c'>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("a"), hana::size_c<0>),
        hana::char_c<'a'>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("ab"), hana::size_c<0>),
        hana::char_c<'a'>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("abc"), hana::size_c<0>),
        hana::char_c<'a'>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("ab"), hana::size_c<1>),
        hana::char_c<'b'>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("abc"), hana::size_c<1>),
        hana::char_c<'b'>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("abcd"), hana::size_c<1>),
        hana::char_c<'b'>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("abc"), hana::size_c<2>),
        hana::char_c<'c'>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(BOOST_HANA_STRING("abcd"), hana::size_c<2>),
        hana::char_c<'c'>
    ));
}
