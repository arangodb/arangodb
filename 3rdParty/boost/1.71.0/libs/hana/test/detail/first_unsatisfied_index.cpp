// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/first_unsatisfied_index.hpp>

#include <boost/hana/not_equal.hpp>
#include <laws/base.hpp>

#include <type_traits>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct poison {
    poison() = default;
    poison(poison const&) = delete;
};

int main() {
    auto predicate = [](auto x) {
        static_assert(!std::is_same<poison, decltype(x)>::value, "");
        return hana::not_equal(x, ct_eq<9>{});
    };

    using Find = hana::detail::first_unsatisfied_index<decltype(predicate)>;

    static_assert(decltype(Find{}())::value == 0, "");

    static_assert(decltype(Find{}(ct_eq<9>{}))::value == 0, "");
    static_assert(decltype(Find{}(ct_eq<0>{}))::value == 1, "");

    static_assert(decltype(Find{}(ct_eq<9>{}, poison{}))::value == 0, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<9>{}))::value == 1, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<1>{}))::value == 2, "");

    static_assert(decltype(Find{}(ct_eq<9>{}, poison{}, poison{}))::value == 0, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<9>{}, poison{}))::value == 1, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<1>{}, ct_eq<9>{}))::value == 2, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))::value == 3, "");

    static_assert(decltype(Find{}(ct_eq<9>{}, poison{}, poison{}, poison{}))::value == 0, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<9>{}, poison{}, poison{}))::value == 1, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<1>{}, ct_eq<9>{}, poison{}))::value == 2, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<9>{}))::value == 3, "");
    static_assert(decltype(Find{}(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}))::value == 4, "");
}
