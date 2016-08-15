// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/string.hpp>

#include <cstring>
namespace hana = boost::hana;


static_assert(hana::is_convertible<hana::string_tag, char const*>{}, "");
static_assert(!hana::is_embedded<hana::string_tag, char const*>{}, "");

int main() {
    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        hana::to<char const*>(BOOST_HANA_STRING("")),
        ""
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        hana::to<char const*>(BOOST_HANA_STRING("a")),
        "a"
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        hana::to<char const*>(BOOST_HANA_STRING("ab")),
        "ab"
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        hana::to<char const*>(BOOST_HANA_STRING("abc")),
        "abc"
    ) == 0);

    BOOST_HANA_RUNTIME_CHECK(std::strcmp(
        hana::to<char const*>(BOOST_HANA_STRING("abcd")),
        "abcd"
    ) == 0);

    // make sure we can turn a non-constexpr hana::string
    // into a constexpr char const*
    auto str = BOOST_HANA_STRING("abcdef");
    constexpr char const* c_str = hana::to<char const*>(str);
    (void)c_str;
}
