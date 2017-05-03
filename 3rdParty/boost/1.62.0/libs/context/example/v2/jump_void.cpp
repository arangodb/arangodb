
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include <boost/context/all.hpp>

namespace ctx = boost::context;

ctx::execution_context< void > f1( ctx::execution_context< void > ctxm) {
    std::cout << "f1: entered first time" << std::endl;
    ctxm = ctxm();
    std::cout << "f1: entered second time" << std::endl;
    return ctxm;
}

int main() {
    ctx::execution_context< void > ctx1( f1);
    ctx1 = ctx1();
    std::cout << "f1: returned first time" << std::endl;
    ctx1 = ctx1();
    std::cout << "f1: returned second time" << std::endl;

    std::cout << "main: done" << std::endl;

    return EXIT_SUCCESS;
}
