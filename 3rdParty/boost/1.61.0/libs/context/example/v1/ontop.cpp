
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include <boost/context/all.hpp>

boost::context::execution_context * ctx1 = nullptr;
boost::context::execution_context * ctx = nullptr;

void f1( void *) {
    std::cout << "f1: entered first time" << std::endl;
    ( * ctx)();
    std::cout << "f1: entered second time" << std::endl;
    ( * ctx)();
    std::cout << "f1: entered third time" << std::endl;
    ( * ctx)();
}

void * f2( void * data) {
    std::cout << "f2: entered" << std::endl;
    return data;
}

int main() {
    boost::context::execution_context ctx1_( f1);
    ctx1 = & ctx1_;
    boost::context::execution_context ctx_( boost::context::execution_context::current() );
    ctx = & ctx_;

    ( * ctx1)();
    std::cout << "f1: returned first time" << std::endl;
    ( * ctx1)();
    std::cout << "f1: returned second time" << std::endl;
    ( * ctx1)( boost::context::exec_ontop_arg, f2);
    std::cout << "f1: returned third time" << std::endl;

    std::cout << "main: done" << std::endl;

    return EXIT_SUCCESS;
}
