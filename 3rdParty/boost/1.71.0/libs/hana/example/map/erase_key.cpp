// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/erase_key.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/type.hpp>

#include <string>
namespace hana = boost::hana;
using namespace std::literals;


int main() {
    auto m = hana::make_map(
        hana::make_pair(hana::type_c<int>, "abcd"s),
        hana::make_pair(hana::type_c<void>, 1234),
        hana::make_pair(BOOST_HANA_STRING("foobar!"), hana::type_c<char>)
    );

    BOOST_HANA_RUNTIME_CHECK(
        hana::erase_key(m, BOOST_HANA_STRING("foobar!")) ==
        hana::make_map(
            hana::make_pair(hana::type_c<int>, "abcd"s),
            hana::make_pair(hana::type_c<void>, 1234)
        )
    );

    BOOST_HANA_RUNTIME_CHECK(hana::erase_key(m, hana::type_c<char>) == m);
}
