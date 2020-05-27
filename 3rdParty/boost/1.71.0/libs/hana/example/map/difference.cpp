// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/difference.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/string.hpp>

#include <string>
namespace hana = boost::hana;


static auto m1 = hana::make_map(
    hana::make_pair(BOOST_HANA_STRING("key1"), hana::type_c<std::string>),
    hana::make_pair(BOOST_HANA_STRING("key2"), hana::type_c<std::string>)
);

static auto m2 = hana::make_map(
    hana::make_pair(BOOST_HANA_STRING("key3"), hana::type_c<std::string>),
    hana::make_pair(BOOST_HANA_STRING("key4"), hana::type_c<std::string>),
    hana::make_pair(BOOST_HANA_STRING("key5"), hana::type_c<std::string>)
);

static auto m3 = hana::make_map(
    hana::make_pair(BOOST_HANA_STRING("key1"), hana::type_c<std::string>),
    hana::make_pair(BOOST_HANA_STRING("key4"), hana::type_c<int>),
    hana::make_pair(BOOST_HANA_STRING("key2"), hana::type_c<long long>)
);

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::difference(m1, m2) == m1);

    BOOST_HANA_CONSTANT_CHECK(hana::difference(m1, m3) == hana::make_map());

    BOOST_HANA_CONSTANT_CHECK(hana::difference(m3, m1) == hana::make_map(
        hana::make_pair(BOOST_HANA_STRING("key4"), hana::type_c<int>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::difference(m2, m3) == hana::make_map(
        hana::make_pair(BOOST_HANA_STRING("key3"), hana::type_c<std::string>),
        hana::make_pair(BOOST_HANA_STRING("key5"), hana::type_c<std::string>)
    ));
}
