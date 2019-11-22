// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/eval.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/lazy.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTEXPR_LAMBDA auto f = hana::make<hana::lazy_tag>([](auto x) {
        return 1 / x;
    });

    BOOST_HANA_CONSTEXPR_LAMBDA auto g = hana::make_lazy([](auto x) {
        return x + 1;
    });

    BOOST_HANA_CONSTEXPR_CHECK(hana::eval(hana::if_(hana::false_c, f(0), g(0))) == 0 + 1);
}
