//  Copyright 2015 Klemens Morgenstern
//
// This file provides a demangling for function names, i.e. entry points of a dll.
//
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DLL_DEMANGLE_SYMBOL_HPP_
#define BOOST_DLL_DEMANGLE_SYMBOL_HPP_

#include <boost/config.hpp>
#include <string>
#include <algorithm>

#if defined(BOOST_MSVC) || defined(BOOST_MSVC_FULL_VER)
#include <boost/detail/winapi/dbghelp.hpp>

namespace boost
{
namespace dll
{
namespace detail
{


inline std::string demangle_symbol(const char *mangled_name)
{
    char unmangled_name[2048];

    ::boost::detail::winapi::
          UnDecorateSymbolName(mangled_name, unmangled_name, 2048, 0);

    return std::string(unmangled_name);
}
inline std::string demangle_symbol(const std::string& mangled_name)
{
    return demangle_symbol(mangled_name.c_str());
}


}}}
#else

#include <boost/core/demangle.hpp>

namespace boost
{
namespace dll
{
namespace detail
{

inline std::string demangle_symbol(const char *mangled_name)
{

    if (*mangled_name == '_')
    {
        //because it start's with an underline _
        auto dm = boost::core::demangle(mangled_name);
        if (!dm.empty())
            return dm;
        else
            return (mangled_name);
    }

    //could not demangled
    return "";


}

//for my personal convinience
inline std::string demangle_symbol(const std::string& mangled_name)
{
    return demangle_symbol(mangled_name.c_str());
}


}}}

#endif

#endif /* INCLUDE_BOOST_DEMANGLE_HPP_ */
