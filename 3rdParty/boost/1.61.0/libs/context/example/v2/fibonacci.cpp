
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <memory>

#include <boost/context/all.hpp>

namespace ctx = boost::context;

int main() {
    int n=35;
    ctx::execution_context< int > source(
        [n](ctx::execution_context< int > sink, int) mutable {
            int a=0;
            int b=1;
            while(n-->0){
                auto result=sink(a);
                sink=std::move(std::get<0>(result));
                auto next=a+b;
                a=b;
                b=next;
            }
            return sink;
        });
    for(int i=0;i<10;++i){
        auto result=source(i);
        source=std::move(std::get<0>(result));
        std::cout<<std::get<1>(result)<<" ";
    }
    std::cout<<std::endl;

    std::cout << "main: done" << std::endl;
}
