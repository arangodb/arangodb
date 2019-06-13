
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <tuple>

#include <boost/context/execution_context.hpp>

namespace ctx = boost::context;

ctx::execution_context< int > f1( ctx::execution_context< int > && ctx, int data) {
    std::cout << "f1: entered first time: " << data  << std::endl;
    std::tie( ctx, data) = ctx( data + 1);
    std::cout << "f1: entered second time: " << data  << std::endl;
    std::tie( ctx, data) = ctx( data + 1);
    std::cout << "f1: entered third time: " << data << std::endl;
    return std::move( ctx);
}

int f2( int data) {
    std::cout << "f2: entered: " << data << std::endl;
    return -1;
}

int main() {
    int data = 0;
    ctx::execution_context< int > ctx( f1);
    std::tie( ctx, data) = ctx( data + 1);
    std::cout << "f1: returned first time: " << data << std::endl;
    std::tie( ctx, data) = ctx( data + 1);
    std::cout << "f1: returned second time: " << data << std::endl;
    std::tie( ctx, data) = ctx( ctx::exec_ontop_arg, f2, data + 1);
    std::cout << "f1: returned third time" << std::endl;

    std::cout << "main: done" << std::endl;

    return EXIT_SUCCESS;
}
