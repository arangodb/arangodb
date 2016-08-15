// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2016 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// MinGW related workaround
#define BOOST_DLL_FORCE_ALIAS_INSTANTIATION

#include <boost/dll.hpp>
#include <string>

#ifdef BOOST_NO_CXX11_NOEXCEPT
#define noexcept
#endif

#define API extern "C" BOOST_SYMBOL_EXPORT

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
//[getting_started_exports_cpp11_function
namespace some_namespace {
    API int i_am_a_cpp11_function(std::string&& param) noexcept;
//          ^--------------------  function name to use in dll::import<>
}
//]
#endif


//[getting_started_exports_cpp_variable
namespace your_project_namespace {
    API std::string cpp_variable_name;
}
//]



//[getting_started_exports_alias
namespace some_namespace {
    std::string i_am_function_with_ugly_name(const std::string& param) noexcept;
}

// When you have no control over function sources or wish to specify another name.
BOOST_DLL_ALIAS(some_namespace::i_am_function_with_ugly_name, pretty_name)
//]

//[getting_started_exports_c_function
API int c_func_name(int);
//]

//[getting_started_exports_c_variable
API int c_variable_name;
//]

int c_func_name(int i) { return ++i; }
int c_variable_name = 1;
std::string your_project_namespace::cpp_variable_name = "some value";

namespace some_namespace {
    std::string i_am_function_with_ugly_name(const std::string& param) noexcept {
        return param + " Hello from lib!";
    }

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    int i_am_a_cpp11_function(std::string&& param) noexcept {
        return static_cast<int>(param.size());
    }
#endif
}

