// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/ebo.hpp>

#include <boost/hana/assert.hpp>

#include <string>
#include <type_traits>
#include <utility>
namespace hana = boost::hana;
using hana::detail::ebo;


template <int> struct empty { };
template <int> struct idx;
template <typename ...Bases> struct inherit : Bases... { };

static_assert(sizeof(inherit<>) == sizeof(inherit<ebo<idx<0>, empty<0>>>), "");
static_assert(sizeof(inherit<>) == sizeof(inherit<ebo<idx<0>, empty<0>>, ebo<idx<1>, empty<1>>>), "");
static_assert(sizeof(inherit<>) == sizeof(inherit<ebo<idx<0>, empty<0>>, ebo<idx<1>, empty<1>>, ebo<idx<2>, empty<2>>>), "");


int main() {
    // Test default-construction
    {
        constexpr ebo<idx<0>, int> e;
        static_assert(hana::detail::ebo_get<idx<0>>(e) == 0, "");
    }

    // Test construction of a non-empty object
    {
        ebo<idx<0>, std::string> e{"foobar"};
        BOOST_HANA_RUNTIME_CHECK(hana::detail::ebo_get<idx<0>>(e) == "foobar");
    }

    {
        ebo<idx<0>, std::string> e{};
        BOOST_HANA_RUNTIME_CHECK(hana::detail::ebo_get<idx<0>>(e) == "");
    }

    // Test construction of a non default-constructible type
    {
        struct nodefault {
            nodefault() = delete;
            explicit nodefault(char const*) { }
        };
        ebo<idx<0>, nodefault> e{"foobar"};
    }

    // Get lvalue, const lvalue and rvalue with a non-empty type
    {
        ebo<idx<0>, std::string> e{"foobar"};
        std::string& s = hana::detail::ebo_get<idx<0>>(e);
        BOOST_HANA_RUNTIME_CHECK(s == "foobar");
        s = "foobaz";
        BOOST_HANA_RUNTIME_CHECK(hana::detail::ebo_get<idx<0>>(e) == "foobaz");
    }

    {
        ebo<idx<0>, std::string> const e{"foobar"};
        std::string const& s = hana::detail::ebo_get<idx<0>>(e);
        BOOST_HANA_RUNTIME_CHECK(s == "foobar");
    }

    {
        ebo<idx<0>, std::string> e{"foobar"};
        std::string&& s = hana::detail::ebo_get<idx<0>>(std::move(e));
        BOOST_HANA_RUNTIME_CHECK(s == "foobar");
    }

    // Get lvalue, const lvalue and rvalue with an empty type
    {
        ebo<idx<0>, empty<0>> e{};
        empty<0>& s = hana::detail::ebo_get<idx<0>>(e);
        (void)s;
    }

    {
        ebo<idx<0>, empty<0>> const e{};
        empty<0> const& s = hana::detail::ebo_get<idx<0>>(e);
        (void)s;
    }

    {
        ebo<idx<0>, empty<0>> e{};
        empty<0>&& s = hana::detail::ebo_get<idx<0>>(std::move(e));
        (void)s;
    }
}
