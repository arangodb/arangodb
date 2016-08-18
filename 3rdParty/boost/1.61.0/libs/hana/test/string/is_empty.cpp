// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/string.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(BOOST_HANA_STRING("")));
    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(hana::string_c<>));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(BOOST_HANA_STRING("a"))));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(BOOST_HANA_STRING("ab"))));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(BOOST_HANA_STRING("abc"))));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(hana::string_c<'a'>)));
}
