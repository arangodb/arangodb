// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


using Function = void();
void function() { }

int main() {
    {
        struct T { };
        T t;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(T{}),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(t),
            hana::type_c<T>
        ));
    }

    // [cv-qualified] reference types
    {
        struct T { };
        T t;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T&>(t)),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T const&>(t)),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T volatile&>(t)),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T const volatile&>(t)),
            hana::type_c<T>
        ));
    }

    // [cv-qualified] rvalue reference types
    {
        struct T { };
        T t;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T&&>(t)),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T const &&>(t)),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T volatile&&>(t)),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(static_cast<T const volatile&&>(t)),
            hana::type_c<T>
        ));
    }

    // typeid_(type_c<T>) is the identity function
    {
        struct T;
        auto const type_const = hana::type_c<T>;
        auto const& type_const_ref = hana::type_c<T>;
        auto& type_ref = hana::type_c<T>;
        auto&& type_ref_ref = static_cast<decltype(type_ref)&&>(type_ref);

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(hana::type_c<T>),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(type_const),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(type_const_ref),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(type_ref),
            hana::type_c<T>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(type_ref_ref),
            hana::type_c<T>
        ));
    }

    // make sure we don't read from non-constexpr variables
    {
        struct T;
        auto t = hana::type_c<T>;
        auto x = 1;
        constexpr auto r1 = hana::typeid_(t); (void)r1;
        constexpr auto r2 = hana::typeid_(x); (void)r2;
    }

    // typeid_ with builtin arrays, function pointers and other weirdos
    {
        struct T { };
        using A = T[3];
        A a;
        A& a_ref = a;
        A const& a_const_ref = a;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(a),
            hana::type_c<A>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(a_ref),
            hana::type_c<A>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(a_const_ref),
            hana::type_c<A>
        ));
    }
    {
        using Fptr = int(*)();
        Fptr f;
        Fptr& f_ref = f;
        Fptr const& f_const_ref = f;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(f),
            hana::type_c<Fptr>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(f_ref),
            hana::type_c<Fptr>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(f_const_ref),
            hana::type_c<Fptr>
        ));
    }
    {
        Function& function_ref = function;

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(function),
            hana::type_c<Function>
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::typeid_(function_ref),
            hana::type_c<Function>
        ));
    }
}
