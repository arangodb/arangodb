// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;


int main() {
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(BOOST_HANA_STRING(""), f),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(BOOST_HANA_STRING("a"), f),
        f(hana::char_c<'a'>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(BOOST_HANA_STRING("ab"), f),
        f(hana::char_c<'a'>, hana::char_c<'b'>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(BOOST_HANA_STRING("abc"), f),
        f(hana::char_c<'a'>, hana::char_c<'b'>, hana::char_c<'c'>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(BOOST_HANA_STRING("abcd"), f),
        f(hana::char_c<'a'>, hana::char_c<'b'>, hana::char_c<'c'>, hana::char_c<'d'>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(BOOST_HANA_STRING("abcde"), f),
        f(hana::char_c<'a'>, hana::char_c<'b'>, hana::char_c<'c'>, hana::char_c<'d'>, hana::char_c<'e'>)
    ));
}
