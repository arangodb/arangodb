// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/back.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/tuple.hpp>

#include <utility>
namespace hana = boost::hana;


// This test checks for move-only friendliness and reference semantics.

struct MoveOnly {
    MoveOnly() = default;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;

    MoveOnly(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly const&) = delete;
};

int main() {
    {
        auto xs = hana::make_tuple(MoveOnly{});
        auto by_val = [](auto) { };

        by_val(std::move(xs));
        by_val(hana::front(std::move(xs)));
        by_val(hana::at_c<0>(std::move(xs)));
        by_val(hana::back(std::move(xs)));
    }

    {
        auto const& xs = hana::make_tuple(MoveOnly{});
        auto by_const_ref = [](auto const&) { };

        by_const_ref(xs);
        by_const_ref(hana::front(xs));
        by_const_ref(hana::at_c<0>(xs));
        by_const_ref(hana::back(xs));
    }

    {
        auto xs = hana::make_tuple(MoveOnly{});
        auto by_ref = [](auto&) { };

        by_ref(xs);
        by_ref(hana::front(xs));
        by_ref(hana::at_c<0>(xs));
        by_ref(hana::back(xs));
    }
}
