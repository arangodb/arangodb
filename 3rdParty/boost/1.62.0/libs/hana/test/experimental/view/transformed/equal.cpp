// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/functional/id.hpp>
#include <boost/hana/not.hpp>

#include <support/seq.hpp>
namespace hana = boost::hana;


int main() {
    auto container = ::seq;

    auto xs = container(0, '1', 2.2);
    auto tr = hana::experimental::transformed(xs, hana::id);

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        tr,
        container()
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        tr,
        container(0)
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        tr,
        container(0, '1')
    )));

    BOOST_HANA_RUNTIME_CHECK(hana::equal(
        tr,
        container(0, '1', 2.2)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        tr,
        container(0, '1', 2.2, 345)
    )));

    BOOST_HANA_RUNTIME_CHECK(hana::not_(hana::equal(
        tr,
        container('0', '1', '2')
    )));
}
