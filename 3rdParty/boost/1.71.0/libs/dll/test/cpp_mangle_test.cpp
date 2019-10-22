// Copyright 2016 Klemens Morgenstern
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/predef.h>

#if (__cplusplus >= 201402L) || (BOOST_COMP_MSVC >= BOOST_VERSION_NUMBER(14,0,0))

#include "../example/b2_workarounds.hpp"

#include <boost/dll/smart_library.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/variant.hpp>

#include <iostream>


struct override_class {};


int main(int argc, char* argv[])
{
    using namespace boost::dll;
    using mangled_storage = detail::mangled_storage_impl;

    boost::dll::fs::path pt = b2_workarounds::first_lib_from_argv(argc, argv);;

    std::cout << "Library: " << pt << std::endl;
    library_info lib{pt};

    mangled_storage ms(lib);

    std::cout << "Symbols: " << std::endl;

    for (auto &s : ms.get_storage())
    {
        std::cout << s.demangled << std::endl;
    }

    std::string v;
    v = ms.get_variable<double>("some_space::variable");

    BOOST_TEST(!v.empty()); //check if a symbols was found.
    BOOST_TEST(v != "some_space::variable"); //demangle is different

    v  = ms.get_variable<double>("some_space::variable_typo");
    BOOST_TEST(v.empty());


    v = ms.get_variable<const double>("unscoped_c_var");

    BOOST_TEST(!v.empty()); //check if a symbols was found.

    v = ms.get_variable<int>("unscoped_var");

    BOOST_TEST(!v.empty()); //check if a symbols was found.


    v = ms.get_function<const int &()>("some_space::scoped_fun");

    BOOST_TEST(!v.empty());
    BOOST_TEST(v != "some_space::scoped_fun");


    auto v1 = ms.get_function<void(const double)>("overloaded");
    auto v2 = ms.get_function<void(const volatile int)>("overloaded");
    BOOST_TEST(!v1.empty());
    BOOST_TEST(!v2.empty());
    BOOST_TEST(v1 != v2);

    v = ms.get_variable<int>("some_space::some_class::value");
    BOOST_TEST(!v.empty());
    BOOST_TEST(v != "some_space::some_class::value");

    v = ms.get_function<void(const int &)>("some_space::some_class::set_value");

    BOOST_TEST(!v.empty());
    BOOST_TEST(v != "some_space::some_class::set_value");



    ms.add_alias<override_class>("some_space::some_class");

    auto ctor1 = ms.get_constructor<override_class()>();
    BOOST_TEST(!ctor1.empty());

    auto ctor2 = ms.get_constructor<override_class(int)>();
    BOOST_TEST(!ctor2.empty());


    v = ms.get_mem_fn<override_class, double(double, double)>("func");
    BOOST_TEST(!v.empty());

    v = ms.get_mem_fn<override_class, int(int, int)>("func");
    BOOST_TEST(!v.empty());


    auto dtor = ms.get_destructor<override_class>();

    BOOST_TEST(!dtor.empty());

    auto var1 = ms.get_function<void(boost::variant<int, double> &)>("use_variant");
    auto var2 = ms.get_function<void(boost::variant<double, int> &)>("use_variant");

    BOOST_TEST(!var1.empty());
    BOOST_TEST(!var2.empty());

// TODO: FIX!
#ifndef BOOST_TRAVISCI_BUILD

#if defined(BOOST_MSVC) || defined(BOOST_MSVC_VER)
    auto vtable = ms.get_vtable<override_class>();
    BOOST_TEST(!vtable.empty());
#else
    auto ti  = ms.get_type_info<override_class>();
    BOOST_TEST(!ti.empty());
#endif

#endif // #ifndef BOOST_TRAVISCI_BUILD

    return boost::report_errors();
}

#else
int main() {return 0;}
#endif
