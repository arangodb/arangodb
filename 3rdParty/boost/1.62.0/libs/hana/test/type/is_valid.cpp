// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/type.hpp>

#include <laws/base.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct undefined { };

struct static_nested_member { static const int member = 1; };
struct static_nested_member_array { static int member[3]; };
struct nested_template_struct { template <typename ...> struct nested; };
struct nested_template_alias { template <typename ...> using nested = void; };

int main() {
    // Check for a non-static member
    {
        struct yes { int member; };
        struct no { };

        auto from_type = hana::is_valid([](auto t) -> decltype(
            hana::traits::declval(t).member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_type(hana::type_c<yes>));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_type(hana::type_c<no>)));

        auto from_object = hana::is_valid([](auto&& t) -> decltype(
            t.member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_object(yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_object(no{})));
    }

    // Check for a static member
    {
        using yes = static_nested_member;
        struct no { };

        auto from_type = hana::is_valid([](auto t) -> decltype(
            decltype(t)::type::member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_type(hana::type_c<yes>));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_type(hana::type_c<no>)));

        auto from_object = hana::is_valid([](auto&& t) -> decltype(
            std::remove_reference_t<decltype(t)>::member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_object(yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_object(no{})));
    }

    // Check for a nested type
    {
        struct yes { using nested = void; };
        struct no { };

        auto from_type = hana::is_valid([](auto t) -> decltype(hana::type_c<
            typename decltype(t)::type::nested
        >) { });
        BOOST_HANA_CONSTANT_CHECK(from_type(hana::type_c<yes>));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_type(hana::type_c<no>)));

        auto from_object = hana::is_valid([](auto&& t) -> decltype(hana::type_c<
            typename std::remove_reference_t<decltype(t)>::nested
        >) { });
        BOOST_HANA_CONSTANT_CHECK(from_object(yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_object(no{})));
    }

    // Check for a nested template
    {
        { // template struct
        using yes = nested_template_struct;
        struct no { };

        auto from_type = hana::is_valid([](auto t) -> decltype(hana::template_<
            decltype(t)::type::template nested
        >) { });
        BOOST_HANA_CONSTANT_CHECK(from_type(hana::type_c<yes>));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_type(hana::type_c<no>)));

        auto from_object = hana::is_valid([](auto&& t) -> decltype(hana::template_<
            std::remove_reference_t<decltype(t)>::template nested
        >) { });
        BOOST_HANA_CONSTANT_CHECK(from_object(yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_object(no{})));
        }

        { // template alias
        using yes = nested_template_alias;
        struct no { };

        auto from_type = hana::is_valid([](auto t) -> decltype(hana::template_<
            decltype(t)::type::template nested
        >) { });
        BOOST_HANA_CONSTANT_CHECK(from_type(hana::type_c<yes>));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_type(hana::type_c<no>)));

        auto from_object = hana::is_valid([](auto&& t) -> decltype(hana::template_<
            std::remove_reference_t<decltype(t)>::template nested
        >) { });
        BOOST_HANA_CONSTANT_CHECK(from_object(yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_object(no{})));
        }
    }

    // Make sure that checking for a nested static or non-static member
    // works even when the type of that member is an array type or
    // something that can't be returned from a function.
    {
        { // non-static member
        struct yes { int member[3]; };
        struct no { };

        auto from_type = hana::is_valid([](auto t) -> decltype(
            (void)hana::traits::declval(t).member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_type(hana::type_c<yes>));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_type(hana::type_c<no>)));

        auto from_object = hana::is_valid([](auto&& t) -> decltype(
            (void)t.member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_object(yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_object(no{})));
        }

        { // static member
        using yes = static_nested_member_array;
        struct no { };

        auto from_type = hana::is_valid([](auto t) -> decltype(
            (void)decltype(t)::type::member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_type(hana::type_c<yes>));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_type(hana::type_c<no>)));

        auto from_object = hana::is_valid([](auto&& t) -> decltype(
            (void)std::remove_reference_t<decltype(t)>::member
        ) { });
        BOOST_HANA_CONSTANT_CHECK(from_object(yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(from_object(no{})));
        }
    }

    // Make sure the result of a `is_valid` function is constexpr
    // even when called on non-constexpr arguments.
    {
        int i;
        auto f = hana::is_valid([](auto) { });
        constexpr auto result = f(i);
        (void)result;
    }

    // Make sure `is_valid` works with non-PODs.
    {
        hana::is_valid(undefined{})(hana::test::Tracked{1});
        hana::is_valid([t = hana::test::Tracked{1}](auto) { return 1; })(hana::test::Tracked{1});
    }

    // Check `is_valid` with a nullary function.
    {
        auto f = [](auto ...x) { (void)sizeof...(x); /* -Wunused-param */ };
        auto g = [](auto ...x) -> char(*)[sizeof...(x)] { };
        BOOST_HANA_CONSTANT_CHECK(hana::is_valid(f)());
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_valid(g)()));
    }

    // Call `is_valid` in the non-curried form.
    {
        struct yes { int member; };
        struct no { };

        auto f = [](auto&& t) -> decltype(t.member) { };

        BOOST_HANA_CONSTANT_CHECK(hana::is_valid(f, yes{}));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_valid(f, no{})));
    }
}
