// Copyright 2016 Klemens Morgenstern
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/config.hpp>

#if (__cplusplus >= 201402L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)

#include "../example/b2_workarounds.hpp"

#include <boost/dll/smart_library.hpp>
#include <boost/dll/import_mangled.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/variant.hpp>
#include <boost/function.hpp>

#include <iostream>

struct override_class
{
    int arr[32];
};


int main(int argc, char* argv[])
{   
     using namespace boost::dll;
    using namespace boost::dll::experimental;
    boost::dll::fs::path pt = b2_workarounds::first_lib_from_argv(argc, argv);

    BOOST_TEST(!pt.empty());
    std::cout << "Library: " << pt << std::endl;

    smart_library sm(pt);

    auto sp_variable = import_mangled<double>(sm, "some_space::variable");

    auto unscoped_var = import_mangled<int>(sm, "unscoped_var");


    auto ovl = import_mangled<void(int), void(double)>(sm, "overloaded");

    ovl(12);
    BOOST_TEST(*unscoped_var == 12);
    ovl(5.0);
    BOOST_TEST(*sp_variable == 5.0);

    boost::function<void(int)> f_test = ovl;//test if it binds
    f_test(-2);
    BOOST_TEST(*unscoped_var == -2);

    sm.add_type_alias<override_class>("some_space::some_class");

    auto func = import_mangled<
            override_class, double(double, double), int(int, int),
            volatile override_class, int(int, int),
            const volatile override_class, double(double, double)>(sm, "func");

    override_class override_class_varible{};

    override_class *ov = &override_class_varible;
    volatile override_class *ovv = ov;
    const volatile override_class *ovcv = ov;

    BOOST_TEST(func(ov, 3.,2.) == 6.);
    BOOST_TEST(func(ov, 1,2  ) == 3 );
    BOOST_TEST(func(ovv, 10,2) == 8 );
    BOOST_TEST(func(ovcv, 9,2) == 4.5 );

    return boost::report_errors();
}

#else
int main() {return 0;}
#endif
