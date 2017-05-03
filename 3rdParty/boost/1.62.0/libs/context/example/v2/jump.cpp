
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include <boost/context/all.hpp>

namespace ctx = boost::context;

ctx::execution_context< int > f1( ctx::execution_context< int > ctxm, int data) {
    std::cout << "f1: entered first time: " << data << std::endl;
    std::tie( ctxm, data) = ctxm( data + 2);
    std::cout << "f1: entered second time: " << data << std::endl;
    return ctxm;
}

int main() {
    int data = 1;
    ctx::execution_context< int > ctx1( f1);
    std::tie( ctx1, data) = ctx1( data + 2);
    std::cout << "f1: returned first time: " << data << std::endl;
    std::tie( ctx1, data) = ctx1( data + 2);
    std::cout << "f1: returned second time: " << data << std::endl;

    std::cout << "main: done" << std::endl;

    return EXIT_SUCCESS;
}
