// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/find.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::char_c<'c'> ^hana::in^ BOOST_HANA_STRING("abcde"));
    BOOST_HANA_CONSTANT_CHECK(!(hana::char_c<'z'> ^hana::in^ BOOST_HANA_STRING("abcde")));

    BOOST_HANA_CONSTANT_CHECK(
        hana::find(BOOST_HANA_STRING("abcxefg"), hana::char_c<'x'>) == hana::just(hana::char_c<'x'>)
    );
}
