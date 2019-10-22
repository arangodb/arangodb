// Copyright 2011-2012 Renato Tegon Forti
// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

// MinGW related workaround
#define BOOST_DLL_FORCE_ALIAS_INSTANTIATION

#include <boost/dll/config.hpp>
#include <boost/dll/alias.hpp>
#include <iostream>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/fusion/container.hpp>

#define LIBRARY_API BOOST_SYMBOL_EXPORT

extern "C" void LIBRARY_API say_hello(void);
extern "C" float LIBRARY_API lib_version(void);
extern "C" int LIBRARY_API increment(int);

extern "C" int LIBRARY_API integer_g;
extern "C" const int LIBRARY_API const_integer_g = 777;

namespace foo {
    std::size_t bar(const std::vector<int>& v) {
        return v.size();
    }

    std::size_t variable = 42;
}



// Make sure that aliases have no problems with memory allocations and different types of input parameters
namespace namespace1 { namespace namespace2 { namespace namespace3 {
    typedef
        boost::fusion::vector<std::vector<int>, std::vector<int>, std::vector<int>, const std::vector<int>*, std::vector<int>* >
    do_share_res_t;

    boost::shared_ptr<do_share_res_t> do_share(
            std::vector<int> v1,
            std::vector<int>& v2,
            const std::vector<int>& v3,
            const std::vector<int>* v4,
            std::vector<int>* v5
        )
    {
        v2.back() = 777;
        v5->back() = 9990;
        return boost::make_shared<do_share_res_t>(v1, v2, v3, v4, v5);
    }

    std::string info("I am a std::string from the test_library (Think of me as of 'Hello world'. Long 'Hello world').");

    int& ref_returning_function() {
        static int i = 0;
        return i;
    }
}}}



BOOST_DLL_ALIAS(foo::bar, foo_bar)
BOOST_DLL_ALIAS(foo::variable, foo_variable)
BOOST_DLL_ALIAS(namespace1::namespace2::namespace3::do_share, do_share)
BOOST_DLL_ALIAS(namespace1::namespace2::namespace3::info, info)
BOOST_DLL_ALIAS(const_integer_g, const_integer_g_alias)
BOOST_DLL_ALIAS(namespace1::namespace2::namespace3::ref_returning_function, ref_returning_function)



int integer_g = 100;

void say_hello(void)
{
   std::cout << "Hello, Boost.Application!" << std::endl;
}

float lib_version(void)
{
   return 1.0;
}

int increment(int n)
{
   return ++n;
}

#include <boost/dll/runtime_symbol_info.hpp>

boost::dll::fs::path this_module_location_from_itself() {
    return boost::dll::this_line_location();
}

BOOST_DLL_ALIAS(this_module_location_from_itself, module_location_from_itself)



int internal_integer_i = 0xFF0000;
extern "C" LIBRARY_API int& reference_to_internal_integer;
int& reference_to_internal_integer = internal_integer_i;

#ifndef BOOST_NO_RVALUE_REFERENCES
extern "C" LIBRARY_API int&& rvalue_reference_to_internal_integer;
int&& rvalue_reference_to_internal_integer = static_cast<int&&>(internal_integer_i);
#endif

