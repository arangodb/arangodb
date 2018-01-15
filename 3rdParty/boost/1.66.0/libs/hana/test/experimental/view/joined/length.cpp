// Copyright Louis Dionne 2013-2017
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
        auto storage1 = container();
        auto storage2 = container();
        auto joined = hana::experimental::joined(storage1, storage2);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(joined),
            hana::size_c<0>
        ));
    }

    {
        auto storage1 = container(undefined<0>{});
        auto storage2 = container();
        auto joined = hana::experimental::joined(storage1, storage2);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(joined),
            hana::size_c<1>
        ));
    }

    {
        auto storage1 = container();
        auto storage2 = container(undefined<0>{});
        auto joined = hana::experimental::joined(storage1, storage2);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(joined),
            hana::size_c<1>
        ));
    }

    {
        auto storage1 = container(undefined<0>{});
        auto storage2 = container(undefined<1>{});
        auto joined = hana::experimental::joined(storage1, storage2);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(joined),
            hana::size_c<2>
        ));
    }

    {
        auto storage1 = container(undefined<0>{});
        auto storage2 = container(undefined<1>{});
        auto joined = hana::experimental::joined(storage1, storage2);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(joined),
            hana::size_c<2>
        ));
    }

    {
        auto storage1 = container(undefined<0>{}, undefined<1>{});
        auto storage2 = container(undefined<2>{});
        auto joined = hana::experimental::joined(storage1, storage2);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(joined),
            hana::size_c<3>
        ));
    }

    {
        auto storage1 = container(undefined<0>{}, undefined<1>{}, undefined<2>{});
        auto storage2 = container(undefined<3>{}, undefined<4>{});
        auto joined = hana::experimental::joined(storage1, storage2);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::length(joined),
            hana::size_c<5>
        ));
    }
}
