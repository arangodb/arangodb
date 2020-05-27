// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_UNFOLDS_HPP
#define BOOST_HANA_TEST_AUTO_UNFOLDS_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/fold_right.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/unfold_left.hpp>
#include <boost/hana/unfold_right.hpp>

#include <laws/base.hpp>
#include <support/minimal_product.hpp>
#include "test_case.hpp"


TestCase test_unfold_left{[]{
    namespace hana = boost::hana;

    hana::test::_injection<0> f{};
    auto stop_at = [=](auto stop) {
        return [=](auto x) {
            return hana::if_(hana::equal(stop, x),
                hana::nothing,
                hana::just(::minimal_product(x + hana::int_c<1>, f(x)))
            );
        };
    };

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_left<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<0>)),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_left<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<1>)),
        MAKE_TUPLE(f(hana::int_c<0>))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_left<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<2>)),
        MAKE_TUPLE(f(hana::int_c<1>), f(hana::int_c<0>))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_left<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<3>)),
        MAKE_TUPLE(f(hana::int_c<2>), f(hana::int_c<1>), f(hana::int_c<0>))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_left<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<4>)),
        MAKE_TUPLE(f(hana::int_c<3>), f(hana::int_c<2>), f(hana::int_c<1>), f(hana::int_c<0>))
    ));
}};


TestCase test_unfold_right{[]{
    namespace hana = boost::hana;

    hana::test::_injection<0> f{};
    auto stop_at = [=](auto stop) {
        return [=](auto x) {
            return hana::if_(hana::equal(stop, x),
                hana::nothing,
                hana::just(::minimal_product(f(x), x + hana::int_c<1>))
            );
        };
    };

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_right<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<0>)),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_right<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<1>)),
        MAKE_TUPLE(f(hana::int_c<0>))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_right<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<2>)),
        MAKE_TUPLE(f(hana::int_c<0>), f(hana::int_c<1>))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_right<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<3>)),
        MAKE_TUPLE(f(hana::int_c<0>), f(hana::int_c<1>), f(hana::int_c<2>))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unfold_right<TUPLE_TAG>(hana::int_c<0>, stop_at(hana::int_c<4>)),
        MAKE_TUPLE(f(hana::int_c<0>), f(hana::int_c<1>), f(hana::int_c<2>), f(hana::int_c<3>))
    ));
}};

// Make sure unfolds can be reversed under certain conditions.
TestCase test_unfold_undo{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    auto z = ct_eq<999>{};
    auto f = ::minimal_product;
    auto g = [=](auto k) {
        return hana::if_(hana::equal(k, z),
            hana::nothing,
            hana::just(k)
        );
    };

    // Make sure the special conditions are met
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        g(z),
        hana::nothing
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        g(f(ct_eq<0>{}, z)),
        hana::just(::minimal_product(ct_eq<0>{}, z))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        g(f(z, ct_eq<0>{})),
        hana::just(::minimal_product(z, ct_eq<0>{}))
    ));

    // Make sure the reversing works
    {
        auto xs = MAKE_TUPLE();
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_left<TUPLE_TAG>(hana::fold_left(xs, z, f), g),
            xs
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_right<TUPLE_TAG>(hana::fold_right(xs, z, f), g),
            xs
        ));
    }
    {
        auto xs = MAKE_TUPLE(ct_eq<0>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_left<TUPLE_TAG>(hana::fold_left(xs, z, f), g),
            xs
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_right<TUPLE_TAG>(hana::fold_right(xs, z, f), g),
            xs
        ));
    }
    {
        auto xs = MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_left<TUPLE_TAG>(hana::fold_left(xs, z, f), g),
            xs
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_right<TUPLE_TAG>(hana::fold_right(xs, z, f), g),
            xs
        ));
    }
    {
        auto xs = MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_left<TUPLE_TAG>(hana::fold_left(xs, z, f), g),
            xs
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unfold_right<TUPLE_TAG>(hana::fold_right(xs, z, f), g),
            xs
        ));
    }
}};

#endif // !BOOST_HANA_TEST_AUTO_UNFOLDS_HPP
