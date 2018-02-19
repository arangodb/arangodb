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

struct override_class
{
    int arr[32];
};


int main(int argc, char* argv[])
{
    using namespace boost::dll;
    using namespace boost::dll::experimental;
    boost::filesystem::path pt = b2_workarounds::first_lib_from_argv(argc, argv);

    BOOST_TEST(!pt.empty());
    std::cout << "Library: " << pt << std::endl;

    smart_library sm(pt);

    auto& unscoped_var = sm.get_variable<int>("unscoped_var");
    BOOST_TEST(unscoped_var == 42);

    auto& unscoped_c_var = sm.get_variable<const double>("unscoped_c_var");
    BOOST_TEST(unscoped_c_var == 1.234);


    auto& sp_variable = sm.get_variable<double>("some_space::variable");
    BOOST_TEST(sp_variable == 0.2);


    auto scoped_fun = sm.get_function<const int&()>("some_space::scoped_fun");
    BOOST_TEST(scoped_fun != nullptr);
    {
        auto &res = scoped_fun();
        const int expected = 0xDEADBEEF;
        BOOST_TEST(res == expected);
    }

    auto ovl1 = sm.get_function<void(int)>   ("overloaded");
    auto ovl2 = sm.get_function<void(double)>("overloaded");

    BOOST_TEST(ovl1 != nullptr);
    BOOST_TEST(ovl2 != nullptr);
    BOOST_TEST(reinterpret_cast<void*>(ovl1) != reinterpret_cast<void*>(ovl2));

    ovl1(12);
    BOOST_TEST(unscoped_var == 12);
    ovl2(5.0);
    BOOST_TEST(sp_variable == 5.0);



    auto var1 = sm.get_function<void(boost::variant<int, double> &)>("use_variant");
    auto var2 = sm.get_function<void(boost::variant<double, int> &)>("use_variant");

    BOOST_TEST(var1 != nullptr);
    BOOST_TEST(var2 != nullptr);
    BOOST_TEST(reinterpret_cast<void*>(var1) != reinterpret_cast<void*>(var2));

    {
         boost::variant<int, double> v1 = 232.22;
         boost::variant<double, int> v2 = -1;

         var1(v1);
         var2(v2);

         struct : boost::static_visitor<void>
         {
             void operator()(double) {BOOST_TEST(false);}
             void operator()(int i) {BOOST_TEST(i == 42);}
         } vis1;

         struct : boost::static_visitor<void>
         {
             void operator()(double d) {BOOST_TEST(d == 3.124);}
             void operator()(int ) {BOOST_TEST(false);}
         } vis2;

         boost::apply_visitor(vis1, v1);
         boost::apply_visitor(vis2, v2);

    }

    /* now test the class stuff */

    //first we import and test the global variables

    auto& father_val = sm.get_variable<int>("some_space::father_value");
    auto& static_val = sm.get_variable<int>("some_space::some_class::value");
    BOOST_TEST(father_val == 12);
    BOOST_TEST(static_val == -1);

    //now get the static function.
    auto set_value = sm.get_function<void(const int &)>("some_space::some_class::set_value");
    BOOST_TEST(set_value != nullptr);

    set_value(42);
    BOOST_TEST(static_val == 42); //alright, static method works.


    //alright, now import the class members
    //first add the type alias.
    sm.add_type_alias<override_class>("some_space::some_class");

    auto set = sm.get_mem_fn<override_class, void(int)>("set");


    try {
        sm.get_mem_fn<override_class, int()>("get");
        BOOST_TEST(false);
    } catch(boost::system::system_error &) {}
    auto get = sm.get_mem_fn<const override_class, int()>("get");

    BOOST_TEST(get != nullptr);
    BOOST_TEST(set != nullptr);

    auto func_dd  = sm.get_mem_fn<override_class,                double(double, double)>("func");
    auto func_ii  = sm.get_mem_fn<override_class,                int(int, int)>         ("func");
    auto func_iiv = sm.get_mem_fn<volatile override_class,       int(int, int)>         ("func");
    auto func_ddc = sm.get_mem_fn<const volatile override_class, double(double, double)>("func");


    BOOST_TEST(func_dd != nullptr);
    BOOST_TEST(func_ii != nullptr);


    auto ctor_v = sm.get_constructor<override_class()>();
    auto ctor_i = sm.get_constructor<override_class(int)>();

    auto dtor   = sm.get_destructor<override_class>();

    //actually never used.
    if (ctor_v.has_allocating())
   {
        //allocate
        auto p = ctor_v.call_allocating();

        //assert it works
        auto val = (p->*get)();
        BOOST_TEST(val == 123);
        //deallocate
        dtor.call_deleting(p);
        //now i cannot assert that it deletes, since it would crash.
   }
    //More tests to assure the correct this-ptr

   typedef override_class * override_class_p;
   override_class_p &this_dll = sm.shared_lib().get<override_class_p>("this_");


   //ok, now load the ctor/dtor
   override_class oc;

   override_class_p this_exe = &oc;

    for (auto& i : oc.arr) {
       i = 0;
    }


    BOOST_TEST((oc.*get)() == 0);           BOOST_TEST(this_dll == this_exe);

    ctor_i.call_standard(&oc, 12);          BOOST_TEST(this_dll == this_exe);

    BOOST_TEST(static_val == 12);
    BOOST_TEST((oc.*get)() == 456);         BOOST_TEST(this_dll == this_exe);
    (oc.*set)(42);
    BOOST_TEST((oc.*get)() == 42);          BOOST_TEST(this_dll == this_exe);


    BOOST_TEST((oc.*func_dd)(3,2)   == 6);  BOOST_TEST(this_dll == this_exe);
    BOOST_TEST((oc.*func_ii)(1,2)   == 3);  BOOST_TEST(this_dll == this_exe);
    BOOST_TEST((oc.*func_ddc)(10,2) == 5);  BOOST_TEST(this_dll == this_exe);
    BOOST_TEST((oc.*func_iiv)(9,2)  == 7);  BOOST_TEST(this_dll == this_exe);

    dtor.call_standard(&oc);                BOOST_TEST(this_dll == this_exe);
    BOOST_TEST(static_val == 0);

    const auto& ti = sm.get_type_info<override_class>();
    BOOST_TEST(ti.name() != nullptr);


    //test the ovls helper.
    {
        namespace ex = boost::dll::experimental;
        auto &var = ex::get<double>(sm, "some_space::variable");
        BOOST_TEST(&var == &sp_variable);

        auto fun = ex::get<void(int)>(sm, "overloaded");
        BOOST_TEST(fun == ovl1);

        auto func_ii  = sm.get_mem_fn<override_class,                int(int, int)>         ("func");

        auto mem_fn = ex::get<override_class, int(int, int)>(sm, "func");

        BOOST_TEST(mem_fn == func_ii);
    }


    return boost::report_errors();
}

#else
int main() {return 0;}
#endif
