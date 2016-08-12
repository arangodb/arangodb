// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/drop_while.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(!hana::is_empty(BOOST_HANA_STRING("abcd")));
    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(BOOST_HANA_STRING("")));

    BOOST_HANA_CONSTANT_CHECK(BOOST_HANA_STRING("abcd")[hana::size_c<2>] == hana::char_c<'c'>);

    auto is_vowel = [](auto c) {
        return c ^hana::in^ BOOST_HANA_STRING("aeiouy");
    };
    BOOST_HANA_CONSTANT_CHECK(
        hana::drop_while(BOOST_HANA_STRING("aioubcd"), is_vowel) == BOOST_HANA_STRING("bcd")
    );
}
