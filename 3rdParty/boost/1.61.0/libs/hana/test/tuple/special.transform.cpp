// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


template <typename ...>
struct F { struct type; };

struct x1;
struct x2;
struct x3;
struct x4;

int main() {
    // transform with tuple_t and a metafunction
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::tuple_t<>, hana::metafunction<F>),
        hana::tuple_t<>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::tuple_t<x1>, hana::metafunction<F>),
        hana::tuple_t<F<x1>::type>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::tuple_t<x1, x2>, hana::metafunction<F>),
        hana::tuple_t<F<x1>::type, F<x2>::type>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::tuple_t<x1, x2, x3>, hana::metafunction<F>),
        hana::tuple_t<F<x1>::type, F<x2>::type, F<x3>::type>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::tuple_t<x1, x2, x3, x4>, hana::metafunction<F>),
        hana::tuple_t<F<x1>::type, F<x2>::type, F<x3>::type, F<x4>::type>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::tuple_t<x1, x2, x3, x4>, hana::template_<F>),
        hana::tuple_t<F<x1>, F<x2>, F<x3>, F<x4>>
    ));
}
