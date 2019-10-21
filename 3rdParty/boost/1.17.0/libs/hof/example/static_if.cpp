/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    static_if.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
/*=============================================================================
    Copyright (c) 2016 Paul Fultz II
    static_if.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include "example.h"

using namespace boost::hof;

// static_if example taken from Baptiste Wicht:
// http://baptiste-wicht.com/posts/2015/07/simulate-static_if-with-c11c14.html

template<typename T>
void decrement_kindof(T& value)
{
    eval(first_of(
        if_(std::is_same<std::string, T>())([&](auto id){
            id(value).pop_back();
        }),
        [&](auto id){
            --id(value);
        }
    ));
}

int main()
{
    std::string s = "hello!";
    decrement_kindof(s);
    assert(s == "hello");

    int i = 4;
    decrement_kindof(i);
    assert(i == 3);
}
