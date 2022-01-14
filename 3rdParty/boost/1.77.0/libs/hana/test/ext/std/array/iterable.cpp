// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/array.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front_exactly.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/iterable.hpp>

#include <array>
namespace hana = boost::hana;


template <int ...i>
constexpr auto array() { return std::array<int, sizeof...(i)>{{i...}}; }

int main() {
    // is_empty
    {
        BOOST_HANA_CONSTANT_CHECK(hana::is_empty(array<>()));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(array<0>())));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(array<0, 1>())));
    }

    // front
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::front(array<0>()) == 0);
        BOOST_HANA_CONSTEXPR_CHECK(hana::front(array<0, 1>()) == 0);
        BOOST_HANA_CONSTEXPR_CHECK(hana::front(array<0, 1, 2>()) == 0);
    }

    // drop_front_exactly
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front_exactly(array<0>()),
            array<>()
        ));
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::drop_front_exactly(array<0, 1>()),
            array<1>()
        ));
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::drop_front_exactly(array<0, 1, 2>()),
            array<1, 2>()
        ));
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::drop_front_exactly(array<0, 1, 2, 3>()),
            array<1, 2, 3>()
        ));


        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::drop_front_exactly(array<0, 1, 2, 3>(), hana::size_c<2>),
            array<2, 3>()
        ));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
            hana::drop_front_exactly(array<0, 1, 2, 3>(), hana::size_c<3>),
            array<3>()
        ));
    }

    // laws
    auto int_arrays = hana::make_tuple(
          array<>()
        , array<0>()
        , array<0, 1>()
        , array<0, 1, 2>()
        , array<0, 1, 2, 3>()
        , array<0, 1, 2, 3, 4>()
    );

    hana::test::TestIterable<hana::ext::std::array_tag>{int_arrays};
}
