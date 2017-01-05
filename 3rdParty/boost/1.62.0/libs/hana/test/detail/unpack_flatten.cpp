// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/detail/unpack_flatten.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    constexpr auto f = hana::test::_injection<0>{};

    {
        auto tuples = hana::make_tuple();
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f()
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple());
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f()
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(), hana::make_tuple());
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f()
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(),
                                       hana::make_tuple(),
                                       hana::make_tuple());
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f()
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(ct_eq<0>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f(ct_eq<0>{})
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(),
                                       hana::make_tuple(ct_eq<0>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f(ct_eq<0>{})
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(),
                                       hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(ct_eq<0>{}),
                                       hana::make_tuple(ct_eq<1>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(ct_eq<0>{}),
                                       hana::make_tuple(ct_eq<1>{}),
                                       hana::make_tuple(ct_eq<2>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        ));
    }

    {
        auto tuples = hana::make_tuple(hana::make_tuple(ct_eq<0>{}),
                                       hana::make_tuple(ct_eq<1>{}),
                                       hana::make_tuple(ct_eq<2>{}, ct_eq<3>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::detail::unpack_flatten(tuples, f),
            f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        ));
    }
}
