/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

//[ is_invocable
#include <type_traits>
#include <boost/callable_traits/is_invocable.hpp>

namespace ct = boost::callable_traits;

struct foo {
    template<typename T>
    typename std::enable_if<std::is_integral<T>::value>::type
    operator()(T){}
};

static_assert(ct::is_invocable<foo, int>::value, "");
static_assert(!ct::is_invocable<foo, double>::value, "");

int main() {}
//]
