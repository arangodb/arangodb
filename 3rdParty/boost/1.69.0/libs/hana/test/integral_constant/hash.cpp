// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


int main() {
    // Unsigned integral constants should hash to `unsigned long long`
    {
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<unsigned char, 10>),
            hana::type_c<hana::integral_constant<unsigned long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<unsigned short, 10>),
            hana::type_c<hana::integral_constant<unsigned long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<unsigned int, 10>),
            hana::type_c<hana::integral_constant<unsigned long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<unsigned long, 10>),
            hana::type_c<hana::integral_constant<unsigned long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<unsigned long long, 10>),
            hana::type_c<hana::integral_constant<unsigned long long, 10>>
        ));
    }

    // Signed integral constants should hash to `signed long long`
    {
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<signed char, 10>),
            hana::type_c<hana::integral_constant<signed long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<signed short, 10>),
            hana::type_c<hana::integral_constant<signed long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<signed int, 10>),
            hana::type_c<hana::integral_constant<signed long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<signed long, 10>),
            hana::type_c<hana::integral_constant<signed long long, 10>>
        ));
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<signed long long, 10>),
            hana::type_c<hana::integral_constant<signed long long, 10>>
        ));
    }

    // `char` should hash to either `signed long long` or `unsigned long long`,
    // depending on its signedness
    {
        using T = std::conditional_t<
            std::is_signed<char>::value,
            signed long long,
            unsigned long long
        >;

        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::integral_c<char, 10>),
            hana::type_c<hana::integral_constant<T, 10>>
        ));
    }

    // Pointers to members should hash to themselves.
    // This test is disabled in pre-C++17, because pointers to non-static
    // data members can't be used as non-type template arguments before that.
    // See http://stackoverflow.com/q/35398848/627587.
    {
#if __cplusplus > 201402L

        struct Foo { int bar; };
        constexpr auto x = hana::integral_constant<int Foo::*, &Foo::bar>{};
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(x),
            hana::type_c<hana::integral_constant<int Foo::*, &Foo::bar>>
        ));

#endif
    }

    // Booleans should hash to themselves
    {
        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::true_c),
            hana::type_c<hana::true_>
        ));

        BOOST_HANA_CONSTANT_ASSERT(hana::equal(
            hana::hash(hana::false_c),
            hana::type_c<hana::false_>
        ));
    }
}
