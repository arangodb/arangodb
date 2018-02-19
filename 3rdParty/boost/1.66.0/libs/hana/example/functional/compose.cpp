// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/functional/compose.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTEXPR_LAMBDA auto to_char = [](int x) {
        return static_cast<char>(x + 48);
    };

    BOOST_HANA_CONSTEXPR_LAMBDA auto increment = [](auto x) {
        return x + 1;
    };

    BOOST_HANA_CONSTEXPR_CHECK(hana::compose(to_char, increment)(3) == '4');
}
