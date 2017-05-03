// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto container = ::seq;
    auto f = hana::test::_injection<0>{};

    {
        auto storage = container();
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f()
        ));
    }

    {
        auto storage = container(ct_eq<0>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f()
        ));
    }{
        auto storage = container(ct_eq<0>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 0>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<0>{})
        ));
    }

    {
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f()
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 0>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<0>{})
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 0, 1>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<0>{}, ct_eq<1>{})
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 1>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<1>{})
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 1, 0>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<1>{}, ct_eq<0>{})
        ));
    }

    {
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f()
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 0>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<0>{})
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 1>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<1>{})
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 2>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<2>{})
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 0, 2>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<0>{}, ct_eq<2>{})
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 1, 1>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<1>{}, ct_eq<1>{})
        ));
    }

    {
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{});
        auto sliced = hana::experimental::sliced(storage, hana::tuple_c<int, 3, 1, 0, 2, 2>);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(sliced, f),
            f(ct_eq<3>{}, ct_eq<1>{}, ct_eq<0>{}, ct_eq<2>{}, ct_eq<2>{})
        ));
    }
}
