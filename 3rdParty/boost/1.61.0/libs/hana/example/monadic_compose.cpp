// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/monadic_compose.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTEXPR_LAMBDA auto block = [](auto ...types) {
        return [=](auto x) {
            return hana::if_(hana::contains(hana::make_tuple(types...), hana::decltype_(x)),
                hana::nothing,
                hana::just(x)
            );
        };
    };

    BOOST_HANA_CONSTEXPR_LAMBDA auto f = block(hana::type_c<double>);
    BOOST_HANA_CONSTEXPR_LAMBDA auto g = block(hana::type_c<int>);
    BOOST_HANA_CONSTEXPR_LAMBDA auto h = hana::monadic_compose(g, f);
    BOOST_HANA_CONSTANT_CHECK(h(1)    == hana::nothing); // fails inside g; 1 has type int
    BOOST_HANA_CONSTANT_CHECK(h(1.2)  == hana::nothing); // fails inside f; 1.2 has type double
    BOOST_HANA_CONSTEXPR_CHECK(h('x') == hana::just('x')); // ok; 'x' has type char
}
