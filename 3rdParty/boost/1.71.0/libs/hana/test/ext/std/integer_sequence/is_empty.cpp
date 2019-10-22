// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>

#include <utility>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(std::index_sequence<>{}));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(std::index_sequence<0>{})));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(std::index_sequence<1>{})));
}
