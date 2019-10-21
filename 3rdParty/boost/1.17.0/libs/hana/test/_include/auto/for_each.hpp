// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_FOR_EACH_HPP
#define BOOST_HANA_TEST_AUTO_FOR_EACH_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/for_each.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>

#include <vector>


TestCase test_for_each{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    // Make sure the function is applied in left-to-right order.
    {
        auto check = [](auto ...xs) {
            std::vector<int> seen{};
            hana::for_each(MAKE_TUPLE(xs...), [&](int x) {
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
        hana::for_each(MAKE_TUPLE(), undefined{});
    }

    // Make sure it works with heterogeneous sequences.
    {
        hana::for_each(MAKE_TUPLE(ct_eq<0>{}), [](auto) { });
        hana::for_each(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), [](auto) { });
        hana::for_each(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), [](auto) { });
        hana::for_each(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), [](auto) { });
    }

    // Make sure for_each is constexpr when used with a constexpr function
    // and constexpr arguments. This used not to be the case.
#ifndef MAKE_TUPLE_NO_CONSTEXPR
    {
        struct f { constexpr void operator()(int) const { } };
        constexpr int i = (hana::for_each(MAKE_TUPLE(1, 2, 3), f{}), 0);
        (void)i;
    }
#endif
}};

#endif // !BOOST_HANA_TEST_AUTO_FOR_EACH_HPP
