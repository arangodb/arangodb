// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/variadic/split_at.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
namespace vd = hana::detail::variadic;
using hana::test::ct_eq;


auto check = [](auto split, auto xs, auto ys) {
    auto result = split([](auto ...xs) {
        return [=](auto ...ys) {
            return hana::make_pair(hana::make_tuple(xs...), hana::make_tuple(ys...));
        };
    });

    BOOST_HANA_CONSTANT_CHECK(hana::equal(xs, hana::first(result)));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(ys, hana::second(result)));
};

int main() {
    {
        check(
            vd::split_at<0>(),
            hana::make_tuple(),
            hana::make_tuple()
        );

        check(
            vd::split_at<0>(ct_eq<1>{}),
            hana::make_tuple(),
            hana::make_tuple(ct_eq<1>{})
        );

        check(
            vd::split_at<0>(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple(),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{})
        );

        check(
            vd::split_at<0>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            hana::make_tuple(),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        );
    }
    {
        check(
            vd::split_at<1>(ct_eq<1>{}),
            hana::make_tuple(ct_eq<1>{}),
            hana::make_tuple()
        );

        check(
            vd::split_at<1>(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple(ct_eq<1>{}),
            hana::make_tuple(ct_eq<2>{})
        );

        check(
            vd::split_at<1>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            hana::make_tuple(ct_eq<1>{}),
            hana::make_tuple(ct_eq<2>{}, ct_eq<3>{})
        );

        check(
            vd::split_at<1>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
            hana::make_tuple(ct_eq<1>{}),
            hana::make_tuple(ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
        );
    }
    {
        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple()
        );

        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple(ct_eq<3>{})
        );

        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple(ct_eq<3>{}, ct_eq<4>{})
        );

        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple(ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{})
        );
    }
    {
        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            hana::make_tuple()
        );

        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            hana::make_tuple(ct_eq<8>{})
        );

        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            hana::make_tuple(ct_eq<8>{}, ct_eq<9>{})
        );

        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            hana::make_tuple(ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{})
        );
    }
    {
        check(
            vd::split_at<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            hana::make_tuple()
        );

        check(
            vd::split_at<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            hana::make_tuple(ct_eq<9>{})
        );

        check(
            vd::split_at<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            hana::make_tuple(ct_eq<9>{}, ct_eq<10>{})
        );
    }
    {
        check(
            vd::split_at<9>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            hana::make_tuple()
        );

        check(
            vd::split_at<9>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            hana::make_tuple(ct_eq<10>{})
        );

        check(
            vd::split_at<9>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}, ct_eq<11>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            hana::make_tuple(ct_eq<10>{}, ct_eq<11>{})
        );
    }
}
