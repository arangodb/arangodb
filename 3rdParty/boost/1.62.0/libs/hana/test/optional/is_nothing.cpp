// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


struct undefined { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::is_nothing(hana::nothing));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_nothing(hana::just(undefined{}))));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_nothing(hana::just(hana::nothing))));
}
