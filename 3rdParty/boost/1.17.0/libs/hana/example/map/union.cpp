// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/union.hpp>

#include <string>
namespace hana = boost::hana;
using namespace hana::literals;


static auto m1 = hana::make_map(
    hana::make_pair(BOOST_HANA_STRING("key1"), hana::type_c<std::string>),
    hana::make_pair(BOOST_HANA_STRING("key2"), hana::type_c<std::string>)
);

static auto m2 = hana::make_map(
    hana::make_pair(BOOST_HANA_STRING("key3"), hana::type_c<std::string>),
    hana::make_pair(BOOST_HANA_STRING("key4"), hana::type_c<std::string>),
    hana::make_pair(BOOST_HANA_STRING("key5"), hana::type_c<std::string>)
);

BOOST_HANA_CONSTANT_CHECK(hana::union_(m1, m2) == hana::make_map(
       hana::make_pair(BOOST_HANA_STRING("key1"), hana::type_c<std::string>),
       hana::make_pair(BOOST_HANA_STRING("key2"), hana::type_c<std::string>),
       hana::make_pair(BOOST_HANA_STRING("key3"), hana::type_c<std::string>),
       hana::make_pair(BOOST_HANA_STRING("key4"), hana::type_c<std::string>),
       hana::make_pair(BOOST_HANA_STRING("key5"), hana::type_c<std::string>)
));

constexpr auto m3 = hana::make_map(
    hana::make_pair(hana::type_c<int>, hana::int_c<1>),
    hana::make_pair(hana::type_c<bool>, hana::bool_c<true>)
);

constexpr auto m4 = hana::make_map(
       hana::make_pair(hana::type_c<char>, hana::char_c<'c'>),
       hana::make_pair(hana::type_c<bool>, hana::bool_c<false>)
);

BOOST_HANA_CONSTANT_CHECK(hana::union_(m3, m4) == hana::make_map(
       hana::make_pair(hana::type_c<int>, hana::int_c<1>),
       hana::make_pair(hana::type_c<bool>, hana::bool_c<false>),
       hana::make_pair(hana::type_c<char>, hana::char_c<'c'>)
));

int main() { }
