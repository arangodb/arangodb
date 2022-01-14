// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/overload_linearly.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct A { };
struct AA : A { };
struct B { };
struct C { };

int main() {
    // 2 functions without overlap
    {
        auto f = hana::overload_linearly(
            [](A) { return ct_eq<0>{}; },
            [](B) { return ct_eq<1>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(A{}),
            ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(AA{}),
            f(A{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(B{}),
            ct_eq<1>{}
        ));
    }

    // 2 functions with overlap
    {
        auto f = hana::overload_linearly(
            [](A) { return ct_eq<0>{}; },
            [](A) { return ct_eq<1>{}; }
        );

        auto g = hana::overload_linearly(
            [](A) { return ct_eq<0>{}; },
            [](AA) { return ct_eq<1>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(A{}),
            ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(AA{}),
            f(A{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            g(A{}),
            ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            g(AA{}),
            g(A{})
        ));
    }

    // 3 functions
    {
        auto f = hana::overload_linearly(
            [](A) { return ct_eq<0>{}; },
            [](B) { return ct_eq<1>{}; },
            [](C) { return ct_eq<2>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(A{}),
            ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(AA{}),
            f(A{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(B{}),
            ct_eq<1>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(C{}),
            ct_eq<2>{}
        ));
    }

    // 1 function (github issue #280)
    {
        auto f = hana::overload_linearly(
            [](A) { return ct_eq<0>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(A{}),
            ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            f(AA{}),
            f(A{})
        ));
    }
}
