// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/integral_constant.hpp>

#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto container = ::seq;
    auto f = hana::test::_injection<0>{};

    {
        auto storage = container();
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<0>),
            container()
        ));
    }

    {
        auto storage = container(ct_eq<0>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<0>),
            container(f(ct_eq<0>{}))
        ));
    }{
        auto storage = container(ct_eq<0>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<1>),
            container()
        ));
    }

    {
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<0>),
            container(f(ct_eq<0>{}), f(ct_eq<1>{}))
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<1>),
            container(f(ct_eq<1>{}))
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<2>),
            container()
        ));
    }

    {
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<0>),
            container(f(ct_eq<0>{}), f(ct_eq<1>{}), f(ct_eq<2>{}))
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<1>),
            container(f(ct_eq<1>{}), f(ct_eq<2>{}))
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<2>),
            container(f(ct_eq<2>{}))
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front(transformed, hana::size_c<3>),
            container()
        ));
    }
}
