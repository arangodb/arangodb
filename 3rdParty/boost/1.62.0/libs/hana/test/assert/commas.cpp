// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>

#include <support/cnumeric.hpp>
namespace hana = boost::hana;


// This test makes sure that we can use conditions with multiple comma-separated
// arguments to the non-MSG versions of the BOOST_HANA_XXX_ASSERT macros.

template <bool value, typename ...>
bool runtime_bool() { return value; }

template <bool value, typename ...>
auto constant_bool() { return make_cnumeric<bool, value>(); }

int main() {
    BOOST_HANA_CONSTANT_ASSERT(constant_bool<true, void>());
    BOOST_HANA_CONSTANT_ASSERT(constant_bool<true, void, void>());
    BOOST_HANA_CONSTANT_ASSERT(constant_bool<true, void, void, void>());

    BOOST_HANA_RUNTIME_ASSERT(runtime_bool<true, void>());
    BOOST_HANA_RUNTIME_ASSERT(runtime_bool<true, void, void>());
    BOOST_HANA_RUNTIME_ASSERT(runtime_bool<true, void, void, void>());

    BOOST_HANA_ASSERT(runtime_bool<true, void>());
    BOOST_HANA_ASSERT(runtime_bool<true, void, void>());
    BOOST_HANA_ASSERT(runtime_bool<true, void, void, void>());
    BOOST_HANA_ASSERT(constant_bool<true, void>());
    BOOST_HANA_ASSERT(constant_bool<true, void, void>());
    BOOST_HANA_ASSERT(constant_bool<true, void, void, void>());
}
