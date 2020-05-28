// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


template <typename ...>
struct F { struct type; };

struct x0;
struct x1;
struct x2;
struct x3;

int main() {
    hana::test::_injection<0> f{};

    // tuple
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::make_tuple(), f),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::make_tuple(ct_eq<0>{}), f),
        f(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}), f),
        f(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), f),
        f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));

    // tuple_t
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<>, f),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0>, f),
        f(hana::type_c<x0>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0, x1>, f),
        f(hana::type_c<x0>, hana::type_c<x1>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0, x1, x2>, f),
        f(hana::type_c<x0>, hana::type_c<x1>, hana::type_c<x2>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0, x1, x2, x3>, f),
        f(hana::type_c<x0>, hana::type_c<x1>, hana::type_c<x2>, hana::type_c<x3>)
    ));

    // tuple_t with metafunction
    auto g = hana::metafunction<F>;
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<>, g),
        g()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0>, g),
        g(hana::type_c<x0>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0, x1>, g),
        g(hana::type_c<x0>, hana::type_c<x1>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0, x1, x2>, g),
        g(hana::type_c<x0>, hana::type_c<x1>, hana::type_c<x2>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(hana::tuple_t<x0, x1, x2, x3>, g),
        g(hana::type_c<x0>, hana::type_c<x1>, hana::type_c<x2>, hana::type_c<x3>)
    ));
}
