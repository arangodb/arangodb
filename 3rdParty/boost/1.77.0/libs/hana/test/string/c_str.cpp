// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/string.hpp>

#include <cstring>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        BOOST_HANA_STRING("").c_str(),
        ""
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        BOOST_HANA_STRING("a").c_str(),
        "a"
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        BOOST_HANA_STRING("ab").c_str(),
        "ab"
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        BOOST_HANA_STRING("abc").c_str(),
        "abc"
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        BOOST_HANA_STRING("abcd").c_str(),
        "abcd"
    ) == 0);

    // make sure we can turn a non-constexpr hana::string
    // into a constexpr char const*
    {
        auto str = BOOST_HANA_STRING("abcdef");
        constexpr char const* c_str = str.c_str();
        (void)c_str;
    }

    // make sure c_str is actually a static member function
    {
        constexpr char const* c_str = hana::string<'f', 'o', 'o'>::c_str();
        (void)c_str;
    }
}
