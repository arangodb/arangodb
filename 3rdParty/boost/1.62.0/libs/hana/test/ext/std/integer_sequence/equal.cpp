// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/not.hpp>

#include <utility>
namespace hana = boost::hana;


using T = int;
using U = long long;

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        std::integer_sequence<T>{},
        std::integer_sequence<U>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        std::integer_sequence<T, 0>{},
        std::integer_sequence<U>{}
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        std::integer_sequence<T>{},
        std::integer_sequence<U, 0>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        std::integer_sequence<T, 0>{},
        std::integer_sequence<U, 0>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        std::integer_sequence<T, 0>{},
        std::integer_sequence<U, 0, 1>{}
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        std::integer_sequence<T, 0, 2>{},
        std::integer_sequence<U, 0, 1>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        std::integer_sequence<T, 0, 1, 2, 3>{},
        std::integer_sequence<U, 0, 1, 2, 3>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        std::integer_sequence<T, 0, 1, 2, 3, 5>{},
        std::integer_sequence<U, 0, 1, 2, 3>{}
    )));
}
