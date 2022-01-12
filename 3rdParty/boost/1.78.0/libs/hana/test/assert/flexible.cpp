// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>

#include <support/cnumeric.hpp>
namespace hana = boost::hana;


template <bool value>
bool runtime_bool() { return value; }

template <bool value>
auto constant_bool() { return make_cnumeric<bool, value>(); }


int main() {
    // Make sure it works at function scope
    BOOST_HANA_ASSERT(runtime_bool<true>());
    BOOST_HANA_ASSERT(constant_bool<true>());

    BOOST_HANA_ASSERT_MSG(runtime_bool<true>(), "message");
    BOOST_HANA_ASSERT_MSG(constant_bool<true>(), "message");

    // Make sure we can reference a local variable
    auto ct_yes = constant_bool<true>();
    BOOST_HANA_ASSERT(ct_yes);
    BOOST_HANA_ASSERT_MSG(ct_yes, "message");

    auto rt_yes = runtime_bool<true>();
    BOOST_HANA_ASSERT(rt_yes);
    BOOST_HANA_ASSERT_MSG(rt_yes, "message");
}
