// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/functional/overload_linearly.hpp>

#include <boost/hana/assert.hpp>

#include <laws/base.hpp>
using namespace boost::hana;


struct A { };
struct AA : A { };
struct B { };
struct C { };

int main() {
    // 2 functions without overlap
    {
        auto f = overload_linearly(
            [](A) { return test::ct_eq<0>{}; },
            [](B) { return test::ct_eq<1>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(A{}),
            test::ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(AA{}),
            f(A{})
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(B{}),
            test::ct_eq<1>{}
        ));
    }

    // 2 functions with overlap
    {
        auto f = overload_linearly(
            [](A) { return test::ct_eq<0>{}; },
            [](A) { return test::ct_eq<1>{}; }
        );

        auto g = overload_linearly(
            [](A) { return test::ct_eq<0>{}; },
            [](AA) { return test::ct_eq<1>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(A{}),
            test::ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(AA{}),
            f(A{})
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            g(A{}),
            test::ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            g(AA{}),
            g(A{})
        ));
    }

    // 3 functions
    {
        auto f = overload_linearly(
            [](A) { return test::ct_eq<0>{}; },
            [](B) { return test::ct_eq<1>{}; },
            [](C) { return test::ct_eq<2>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(A{}),
            test::ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(AA{}),
            f(A{})
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(B{}),
            test::ct_eq<1>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(C{}),
            test::ct_eq<2>{}
        ));
    }

    // 1 function (github issue #280)
    {
        auto f = overload_linearly(
            [](A) { return test::ct_eq<0>{}; }
        );

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(A{}),
            test::ct_eq<0>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            f(AA{}),
            f(A{})
        ));
    }
}
