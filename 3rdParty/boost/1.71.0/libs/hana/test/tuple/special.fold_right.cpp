// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fold_right.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


template <typename ...>
struct F { struct type; };

struct x0;
struct x1;
struct x2;
struct x3;

int main() {
    // tuple_t and an initial state
    {
        auto f = hana::metafunction<F>;
        auto s = hana::type_c<struct initial_state>;
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<>, s, f),
            s
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0>, s, f),
            f(hana::type_c<x0>, s)
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0, x1>, s, f),
            f(hana::type_c<x0>, f(hana::type_c<x1>, s))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0, x1, x2>, s, f),
            f(hana::type_c<x0>, f(hana::type_c<x1>, f(hana::type_c<x2>, s)))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0, x1, x2, x3>, s, f),
            f(hana::type_c<x0>, f(hana::type_c<x1>, f(hana::type_c<x2>, f(hana::type_c<x3>, s))))
        ));
    }

    // tuple_t and no initial state
    {
        auto f = hana::metafunction<F>;
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0>, f),
            hana::type_c<x0>
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0, x1>, f),
            f(hana::type_c<x0>, hana::type_c<x1>)
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0, x1, x2>, f),
            f(hana::type_c<x0>, f(hana::type_c<x1>, hana::type_c<x2>))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::fold_right(hana::tuple_t<x0, x1, x2, x3>, f),
            f(hana::type_c<x0>, f(hana::type_c<x1>, f(hana::type_c<x2>, hana::type_c<x3>)))
        ));
    }
}
