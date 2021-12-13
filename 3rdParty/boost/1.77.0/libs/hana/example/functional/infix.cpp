// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/infix.hpp>
#include <boost/hana/pair.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTEXPR_LAMBDA auto divmod = hana::infix([](auto x, auto y) {
    // this could be a more efficient implementation
    return hana::make_pair(x / y, x % y);
});

int main() {
    BOOST_HANA_CONSTEXPR_CHECK((42 ^divmod^ 23) == hana::make_pair(1, 19));
}
