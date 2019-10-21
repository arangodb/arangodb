// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/not.hpp>

#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_ord;


int main() {
    auto container = ::seq;
    auto f = hana::test::_injection<0>{};

    {
        auto storage = container();
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::less(
            transformed,
            container()
        )));
    }{
        auto storage = container(ct_ord<0>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::less(
            container(),
            transformed
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::less(
            container(f(ct_ord<0>{})),
            transformed
        )));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::less(
            transformed,
            container()
        )));
    }{
        auto storage = container(ct_ord<0>{}, ct_ord<1>{});
        auto transformed = hana::experimental::transformed(storage, f);
        BOOST_HANA_CONSTANT_CHECK(hana::less(
            container(),
            transformed
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::less(
            container(f(ct_ord<0>{})),
            transformed
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::less(
            container(f(ct_ord<0>{}), f(ct_ord<0>{})),
            transformed
        ));
    }
}
