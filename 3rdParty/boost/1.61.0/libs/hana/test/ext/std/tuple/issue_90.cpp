// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/back.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/integral_constant.hpp>

#include <tuple>
#include <utility>
namespace hana = boost::hana;


template <typename T>
T const& cref(T& t) { return t; }

// a non-movable, non-copyable type
struct RefOnly {
    RefOnly() = default;
    RefOnly(RefOnly const&) = delete;
    RefOnly(RefOnly&&) = delete;
};

template <int i>
struct RefOnly_i : hana::int_<i> {
    RefOnly_i() = default;
    RefOnly_i(RefOnly_i const&) = delete;
    RefOnly_i(RefOnly_i&&) = delete;
};

int main() {
    std::tuple<RefOnly> t;

    // Make sure that we return the proper reference types from `at`.
    {
        RefOnly&& r1 = hana::at_c<0>(std::move(t));
        RefOnly& r2 = hana::at_c<0>(t);
        RefOnly const& r3 = hana::at_c<0>(cref(t));

        (void)r1; (void)r2; (void)r3;
    }

    // Make sure we return the proper reference types from `front`.
    {
        RefOnly&& r1 = hana::front(std::move(t));
        RefOnly& r2 = hana::front(t);
        RefOnly const& r3 = hana::front(cref(t));

        (void)r1; (void)r2; (void)r3;
    }

    // Make sure we return the proper reference types from `back`.
    {
        RefOnly&& r1 = hana::back(std::move(t));
        RefOnly& r2 = hana::back(t);
        RefOnly const& r3 = hana::back(cref(t));

        (void)r1; (void)r2; (void)r3;
    }

    // Make sure we return the proper reference types from `at_key`.
    {
        std::tuple<RefOnly_i<3>> t{};
        RefOnly_i<3>& r1 = hana::at_key(t, RefOnly_i<3>{});
        RefOnly_i<3> const& r2 = hana::at_key(cref(t), RefOnly_i<3>{});
        RefOnly_i<3>&& r3 = hana::at_key(std::move(t), RefOnly_i<3>{});

        (void)r1; (void)r2; (void)r3;
    }
}
