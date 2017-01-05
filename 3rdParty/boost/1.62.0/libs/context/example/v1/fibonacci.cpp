
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <memory>

#include <boost/context/all.hpp>

int main() {
    int n=35;
    boost::context::execution_context sink( boost::context::execution_context::current() );
    boost::context::execution_context source(
        [n,&sink](void*)mutable{
            int a=0;
            int b=1;
            while(n-->0){
                sink(&a);
                auto next=a+b;
                a=b;
                b=next;
            }
        });
    for(int i=0;i<10;++i){
        std::cout<<*(int*)source()<<" ";
    }
    std::cout << "\nmain: done" << std::endl;
    return EXIT_SUCCESS;
}
