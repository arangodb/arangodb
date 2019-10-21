// Copyright (c) 2018 Oxford Nanopore Technologies
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

int main(int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i) {
        std::cout << argv[i] << '\n';
    }
    return argc;
}
