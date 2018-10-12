// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/types.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;


template <typename ...>
struct mf { struct type; };

template <int> struct x;

int main() {
    // with a Metafunction
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<>{}, hana::metafunction<mf>),
            hana::type_c<mf<>::type>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>>{}, hana::metafunction<mf>),
            hana::type_c<mf<x<0>>::type>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>, x<1>>{}, hana::metafunction<mf>),
            hana::type_c<mf<x<0>, x<1>>::type>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>, x<1>, x<2>>{}, hana::metafunction<mf>),
            hana::type_c<mf<x<0>, x<1>, x<2>>::type>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::metafunction<mf>),
            hana::type_c<mf<x<0>, x<1>, x<2>, x<3>>::type>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>, x<1>, x<2>, x<3>, x<4>>{}, hana::metafunction<mf>),
            hana::type_c<mf<x<0>, x<1>, x<2>, x<3>, x<4>>::type>
        ));
    }

    // with a non-Metafunction
    {
        auto f = hana::test::_injection<0>{};

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<>{}, f),
            f()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>>{}, f),
            f(hana::type_c<x<0>>)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>, x<1>>{}, f),
            f(hana::type_c<x<0>>, hana::type_c<x<1>>)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>, x<1>, x<2>>{}, f),
            f(hana::type_c<x<0>>, hana::type_c<x<1>>, hana::type_c<x<2>>)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, f),
            f(hana::type_c<x<0>>, hana::type_c<x<1>>, hana::type_c<x<2>>, hana::type_c<x<3>>)
        ));
    }
}
