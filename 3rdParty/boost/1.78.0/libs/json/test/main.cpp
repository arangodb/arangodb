//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#include "test_suite.hpp"

#include <iostream>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

// Simple main used to produce stand
// alone executables that run unit tests.
int main(int argc, char const* const* argv)
{
#ifdef _MSC_VER
    {
        int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        flags |= _CRTDBG_LEAK_CHECK_DF;
        _CrtSetDbgFlag(flags);
    }
#endif

    ::test_suite::debug_stream log(std::cerr);
    return ::test_suite::run(log, argc, argv);
}
