// Copyright Antony Polukhin, 2016-2018.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#include <string>
#include <cstring>
#include <windows.h>
#include "dbgeng.h"

#ifdef BOOST_NO_CXX11_THREAD_LOCAL
#   error Your compiler does not support C++11 thread_local storage. It`s impossible to build with BOOST_STACKTRACE_USE_WINDBG_CACHED.
#endif

int foo() {
    static thread_local std::string i = std::string();
    
    return static_cast<int>(i.size());
}

int main() {
    ::CoInitializeEx(0, COINIT_MULTITHREADED);
    
    return foo();
}
