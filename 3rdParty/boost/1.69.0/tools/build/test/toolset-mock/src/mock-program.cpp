// mock-program.cpp
//
// Copyright (c) 2017 Steven Watanabe
//
// Distributed under the Boost Software License Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This program does nothing except to exec a python script

#include <vector>
#include <iostream>
#include <stdio.h>
#include "config.h"

#if defined(_WIN32)
    #include <process.h>
    #define execv _execv
#else
    #include <unistd.h>
#endif

#ifndef PY_SCRIPT
#error PY_SCRIPT must be defined to the absolute path to the script to run
#endif

#ifndef PYTHON_CMD
#error PYTHON_CMD must be defined to the absolute path to the python interpreter
#endif

int main(int argc, char ** argv)
{
    std::vector<char *> args;
    char python_cmd[] = PYTHON_CMD;
    char script[] = PY_SCRIPT;
    args.push_back(python_cmd);
    args.push_back(script);
    args.insert(args.end(), argv + 1, argv + argc);
    args.push_back(NULL);
    execv(python_cmd, &args[0]);
    perror("exec");
}
