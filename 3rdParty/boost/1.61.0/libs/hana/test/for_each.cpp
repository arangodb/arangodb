// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/for_each.hpp>

#include <support/seq.hpp>
#include <laws/base.hpp>

#include <vector>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct constexpr_function {
    template <typename T>
    constexpr void operator()(T) const { }
};

constexpr auto list = ::seq;

int main() {
    // Make sure the function is applied in left-to-right order.
    {
        auto check = [](auto ...xs) {
            std::vector<int> seen{};
            hana::for_each(list(xs...), [&](int x) {
                seen.push_back(x);
            });
            BOOST_HANA_RUNTIME_CHECK(seen == std::vector<int>{xs...});
        };

        check();
        check(0);
        check(0, 1);
        check(0, 1, 2);
        check(0, 1, 2, 3);
        check(0, 1, 2, 3, 4);
    }

    // Make sure the function is never called when the sequence is empty.
    {
        struct undefined { };
        hana::for_each(list(), undefined{});
    }

    // Make sure it works with heterogeneous sequences.
    {
        hana::for_each(list(ct_eq<0>{}), [](auto) { });
        hana::for_each(list(ct_eq<0>{}, ct_eq<1>{}), [](auto) { });
        hana::for_each(list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), [](auto) { });
        hana::for_each(list(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), [](auto) { });
    }

    // Make sure for_each is constexpr when used with a constexpr function
    // and constexpr arguments. This used not to be the case.
    {
        constexpr int i = (hana::for_each(list(1, 2, 3), constexpr_function{}), 0);
        (void)i;
    }
}
