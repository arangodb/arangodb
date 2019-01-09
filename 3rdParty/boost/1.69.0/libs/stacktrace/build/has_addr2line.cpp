// Copyright Antony Polukhin, 2016-2018.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <string>

#include <boost/config.hpp>
#include <unwind.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {

#ifdef BOOST_STACKTRACE_ADDR2LINE_LOCATION
    std::string s = BOOST_STRINGIZE( BOOST_STACKTRACE_ADDR2LINE_LOCATION );
    s += " -h";
#else
    std::string s = "/usr/bin/addr2line -h";
#endif

    return std::system(s.c_str());
}
