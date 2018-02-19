// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTEXPR_LAMBDA auto incr = [](auto x) -> decltype(x + 1) {
        return x + 1;
    };

    BOOST_HANA_CONSTEXPR_CHECK(hana::sfinae(incr)(1) == hana::just(2));

    struct invalid { };
    BOOST_HANA_CONSTANT_CHECK(hana::sfinae(incr)(invalid{}) == hana::nothing);
}
