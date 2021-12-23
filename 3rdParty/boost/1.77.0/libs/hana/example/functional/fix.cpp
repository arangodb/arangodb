// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/functional/fix.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTEXPR_STATELESS_LAMBDA auto factorial = hana::fix([](auto fact, auto n) -> int {
    if (n == 0) return 1;
    else        return n * fact(n - 1);
});

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(factorial(5) == 120);
}
