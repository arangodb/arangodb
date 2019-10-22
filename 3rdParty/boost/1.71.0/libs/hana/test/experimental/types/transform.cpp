// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/types.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


template <typename ...>
struct mf { struct type; };

template <int> struct x;
struct undefined { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(hana::experimental::types<>{}, undefined{}),
        hana::experimental::types<>{}
    ));

    // with a Metafunction
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>>{}, hana::metafunction<mf>),
            hana::experimental::types<mf<x<0>>::type>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>, x<1>>{}, hana::metafunction<mf>),
            hana::experimental::types<mf<x<0>>::type, mf<x<1>>::type>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>, x<1>, x<2>>{}, hana::metafunction<mf>),
            hana::experimental::types<mf<x<0>>::type, mf<x<1>>::type, mf<x<2>>::type>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::metafunction<mf>),
            hana::experimental::types<mf<x<0>>::type, mf<x<1>>::type, mf<x<2>>::type, mf<x<3>>::type>{}
        ));
    }

    // with a non-Metafunction
    {
        auto f = [](auto t) {
            return hana::metafunction<mf>(t);
        };

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>>{}, f),
            hana::experimental::types<mf<x<0>>::type>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>, x<1>>{}, f),
            hana::experimental::types<mf<x<0>>::type, mf<x<1>>::type>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>, x<1>, x<2>>{}, f),
            hana::experimental::types<mf<x<0>>::type, mf<x<1>>::type, mf<x<2>>::type>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::transform(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, f),
            hana::experimental::types<mf<x<0>>::type, mf<x<1>>::type, mf<x<2>>::type, mf<x<3>>::type>{}
        ));
    }
}
