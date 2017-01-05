// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/concept/logical.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/logical.hpp>

#include <vector>
using namespace boost::hana;


int main() {
    test::TestLogical<bool>{make<tuple_tag>(true, false)};

    // eval_if
    {
        BOOST_HANA_CONSTEXPR_CHECK(equal(
            eval_if(true, always(1), always(2)),
            1
        ));

        BOOST_HANA_CONSTEXPR_CHECK(equal(
            eval_if(false, always(1), always(2)),
            2
        ));
    }

    // not_
    {
        BOOST_HANA_CONSTEXPR_CHECK(equal(not_(true), false));
        BOOST_HANA_CONSTEXPR_CHECK(equal(not_(false), true));
    }

    // while_
    {
        auto less_than = [](auto n) {
            return [n](auto v) { return v.size() < n; };
        };
        auto f = [](auto v) {
            v.push_back(v.size());
            return v;
        };

        BOOST_HANA_RUNTIME_CHECK(equal(
            while_(less_than(0u), std::vector<int>{}, f),
            std::vector<int>{}
        ));

        BOOST_HANA_RUNTIME_CHECK(equal(
            while_(less_than(1u), std::vector<int>{}, f),
            std::vector<int>{0}
        ));

        BOOST_HANA_RUNTIME_CHECK(equal(
            while_(less_than(2u), std::vector<int>{}, f),
            std::vector<int>{0, 1}
        ));

        BOOST_HANA_RUNTIME_CHECK(equal(
            while_(less_than(3u), std::vector<int>{}, f),
            std::vector<int>{0, 1, 2}
        ));

        BOOST_HANA_RUNTIME_CHECK(equal(
            while_(less_than(4u), std::vector<int>{}, f),
            std::vector<int>{0, 1, 2, 3}
        ));

        // Make sure it can be called with an lvalue state:
        std::vector<int> v{};
        BOOST_HANA_RUNTIME_CHECK(equal(
            while_(less_than(4u), v, f),
            std::vector<int>{0, 1, 2, 3}
        ));
    }
}
