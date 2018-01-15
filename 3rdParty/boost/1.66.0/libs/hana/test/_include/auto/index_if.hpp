// Copyright Louis Dionne 2013-2017
// Copyright Jason Rice 2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_INDEX_IF_HPP
#define BOOST_HANA_TEST_AUTO_INDEX_IF_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/index_if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/tracked.hpp>


namespace _test_index_if_detail { template <int> struct invalid { }; }


TestCase test_index_if{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;
    using _test_index_if_detail::invalid;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::index_if(MAKE_TUPLE(), hana::equal.to(ct_eq<0>{})),
        hana::nothing
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::index_if(MAKE_TUPLE(ct_eq<0>{}), hana::equal.to(ct_eq<0>{})),
        hana::just(hana::size_c<0>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::index_if(MAKE_TUPLE(ct_eq<0>{}), hana::equal.to(ct_eq<42>{})),
        hana::nothing
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::index_if(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(ct_eq<0>{})),
        hana::just(hana::size_c<0>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::index_if(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(ct_eq<1>{})),
        hana::just(hana::size_c<1>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::index_if(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(ct_eq<2>{})),
        hana::just(hana::size_c<2>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::index_if(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(ct_eq<42>{})),
        hana::nothing
    ));

#ifndef MAKE_TUPLE_NO_CONSTEXPR
    auto type_equal = [](auto type) { return [=](auto&& value) {
        return hana::equal(hana::typeid_(value), type);
    };};

    static_assert(decltype(hana::equal(
        hana::index_if(MAKE_TUPLE(1, '2', 3.3), type_equal(hana::type_c<int>)),
        hana::just(hana::size_c<0>)
    )){}, "");
    static_assert(decltype(hana::equal(
        hana::index_if(MAKE_TUPLE(1, '2', 3.3), type_equal(hana::type_c<char>)),
        hana::just(hana::size_c<1>)
    )){}, "");
    static_assert(decltype(hana::equal(
        hana::index_if(MAKE_TUPLE(1, '2', 3.3), type_equal(hana::type_c<double>)),
        hana::just(hana::size_c<2>)
    )){}, "");
    static_assert(decltype(hana::equal(
        hana::index_if(MAKE_TUPLE(1, '2', 3.3), type_equal(hana::type_c<invalid<42>>)),
        hana::nothing
    )){}, "");
#endif
}};

#endif // !BOOST_HANA_TEST_AUTO_INDEX_IF_HPP
