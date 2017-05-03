// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


// This test checks that the unary + operator on types works as expected.
// The unary + operator is supposed to turn a reference to a hana::type
// object into an equivalent prvalue.

struct T;

int main() {
    auto& ref = hana::type_c<T>;
    auto const& cref = hana::type_c<T>;
    auto&& rref = hana::type_c<T>;
    auto val = hana::type_c<T>;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(val, +val));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(val, +ref));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(val, +cref));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(val, +rref));

    static_assert(!std::is_reference<decltype(+val)>{}, "");
    static_assert(!std::is_reference<decltype(+ref)>{}, "");
    static_assert(!std::is_reference<decltype(+cref)>{}, "");
    static_assert(!std::is_reference<decltype(+rref)>{}, "");

    using T1 = decltype(+val)::type;
    using T2 = decltype(+ref)::type;
    using T3 = decltype(+cref)::type;
    using T4 = decltype(+rref)::type;
}
