// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/length.hpp>

#include <support/seq.hpp>
namespace hana = boost::hana;


template <int> struct undefined { };

int main() {
    auto container = ::seq;

    {
        auto storage = container();
        auto transformed = hana::experimental::transformed(storage, undefined<99>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(transformed),
            hana::size_c<0>
        ));
    }{
        auto storage = container(undefined<0>{});
        auto transformed = hana::experimental::transformed(storage, undefined<99>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(transformed),
            hana::size_c<1>
        ));
    }{
        auto storage = container(undefined<0>{}, undefined<1>{});
        auto transformed = hana::experimental::transformed(storage, undefined<99>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(transformed),
            hana::size_c<2>
        ));
    }{
        auto storage = container(undefined<0>{}, undefined<1>{}, undefined<2>{});
        auto transformed = hana::experimental::transformed(storage, undefined<99>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(transformed),
            hana::size_c<3>
        ));
    }
}
