// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>

#include <support/cnumeric.hpp>
namespace hana = boost::hana;


// This test makes sure that we can use lambdas inside the various
// BOOST_HANA_XXX_ASSERT macros.

template <bool value>
bool runtime_bool() { return value; }

template <bool value>
auto constant_bool() { return make_cnumeric<bool, value>(); }

int main() {
    BOOST_HANA_CONSTANT_ASSERT([]{ return constant_bool<true>(); }());
    BOOST_HANA_RUNTIME_ASSERT([]{ return runtime_bool<true>(); }());
    BOOST_HANA_ASSERT([] { return constant_bool<true>(); }());
    BOOST_HANA_ASSERT([] { return runtime_bool<true>(); }());
}
