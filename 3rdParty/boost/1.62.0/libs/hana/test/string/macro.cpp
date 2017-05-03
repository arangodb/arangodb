// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/string.hpp>

#include <type_traits>
namespace hana = boost::hana;


int main() {
    // make sure string_c and BOOST_HANA_STRING give the same result

    {
        auto const s1 = BOOST_HANA_STRING("");
        auto const s2 = hana::string_c<>;
        static_assert(std::is_same<decltype(s1), decltype(s2)>::value, "");
    }
    {
        auto const s1 = BOOST_HANA_STRING("a");
        auto const s2 = hana::string_c<'a'>;
        static_assert(std::is_same<decltype(s1), decltype(s2)>::value, "");
    }
    {
        auto const s1 = BOOST_HANA_STRING("abcd");
        auto const s2 = hana::string_c<'a', 'b', 'c', 'd'>;
        static_assert(std::is_same<decltype(s1), decltype(s2)>::value, "");
    }
}
