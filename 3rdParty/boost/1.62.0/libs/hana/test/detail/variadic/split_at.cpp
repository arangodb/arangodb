// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/variadic/split_at.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
using namespace boost::hana;


auto check = [](auto split, auto xs, auto ys) {
    auto result = split([](auto ...xs) {
        return [=](auto ...ys) {
            return make_pair(make_tuple(xs...), make_tuple(ys...));
        };
    });

    BOOST_HANA_CONSTANT_CHECK(equal(xs, first(result)));
    BOOST_HANA_CONSTANT_CHECK(equal(ys, second(result)));
};

int main() {
    namespace vd = detail::variadic;
    using test::ct_eq;

    {
        check(
            vd::split_at<0>(),
            make<tuple_tag>(),
            make<tuple_tag>()
        );

        check(
            vd::split_at<0>(ct_eq<1>{}),
            make<tuple_tag>(),
            make<tuple_tag>(ct_eq<1>{})
        );

        check(
            vd::split_at<0>(ct_eq<1>{}, ct_eq<2>{}),
            make<tuple_tag>(),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{})
        );

        check(
            vd::split_at<0>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            make<tuple_tag>(),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        );
    }
    {
        check(
            vd::split_at<1>(ct_eq<1>{}),
            make<tuple_tag>(ct_eq<1>{}),
            make<tuple_tag>()
        );

        check(
            vd::split_at<1>(ct_eq<1>{}, ct_eq<2>{}),
            make<tuple_tag>(ct_eq<1>{}),
            make<tuple_tag>(ct_eq<2>{})
        );

        check(
            vd::split_at<1>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            make<tuple_tag>(ct_eq<1>{}),
            make<tuple_tag>(ct_eq<2>{}, ct_eq<3>{})
        );

        check(
            vd::split_at<1>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
            make<tuple_tag>(ct_eq<1>{}),
            make<tuple_tag>(ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
        );
    }
    {
        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}),
            make<tuple_tag>()
        );

        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}),
            make<tuple_tag>(ct_eq<3>{})
        );

        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}),
            make<tuple_tag>(ct_eq<3>{}, ct_eq<4>{})
        );

        check(
            vd::split_at<2>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}),
            make<tuple_tag>(ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{})
        );
    }
    {
        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            make<tuple_tag>()
        );

        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            make<tuple_tag>(ct_eq<8>{})
        );

        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            make<tuple_tag>(ct_eq<8>{}, ct_eq<9>{})
        );

        check(
            vd::split_at<7>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
            make<tuple_tag>(ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{})
        );
    }
    {
        check(
            vd::split_at<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            make<tuple_tag>()
        );

        check(
            vd::split_at<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            make<tuple_tag>(ct_eq<9>{})
        );

        check(
            vd::split_at<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
            make<tuple_tag>(ct_eq<9>{}, ct_eq<10>{})
        );
    }
    {
        check(
            vd::split_at<9>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            make<tuple_tag>()
        );

        check(
            vd::split_at<9>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            make<tuple_tag>(ct_eq<10>{})
        );

        check(
            vd::split_at<9>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}, ct_eq<10>{}, ct_eq<11>{}),
            make<tuple_tag>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            make<tuple_tag>(ct_eq<10>{}, ct_eq<11>{})
        );
    }
}
