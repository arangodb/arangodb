// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/string.hpp>

#include <type_traits>
namespace hana = boost::hana;


constexpr char const empty[] = "";
constexpr char const a[] = "a";
constexpr char const ab[] = "ab";
constexpr char const abc[] = "abc";
constexpr char const abcd[] = "abcd";

int main() {
    {
        auto string = hana::to<hana::string_tag>(hana::integral_constant<char const*, empty>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(string, hana::string_c<>));
    }

    {
        auto string = hana::to<hana::string_tag>(hana::integral_constant<char const*, a>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(string, hana::string_c<'a'>));
    }

    {
        auto string = hana::to<hana::string_tag>(hana::integral_constant<char const*, ab>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(string, hana::string_c<'a', 'b'>));
    }

    {
        auto string = hana::to<hana::string_tag>(hana::integral_constant<char const*, abc>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(string, hana::string_c<'a', 'b', 'c'>));
    }

    {
        auto string = hana::to<hana::string_tag>(hana::integral_constant<char const*, abcd>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(string, hana::string_c<'a', 'b', 'c', 'd'>));
    }

    // Make sure it also works with std::integral_constant, for example
    {
        auto string = hana::to<hana::string_tag>(std::integral_constant<char const*, abcd>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(string, hana::string_c<'a', 'b', 'c', 'd'>));
    }

    // Make sure the `to_string` shortcut works
    {
        auto string = hana::to_string(hana::integral_constant<char const*, abcd>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(string, hana::string_c<'a', 'b', 'c', 'd'>));
    }
}
