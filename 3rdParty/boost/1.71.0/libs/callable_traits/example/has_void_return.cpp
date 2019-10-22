/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

//[ has_void_return
#include <type_traits>
#include <boost/callable_traits/has_void_return.hpp>

namespace ct = boost::callable_traits;

static_assert(ct::has_void_return<void()>::value, "");
static_assert(!ct::has_void_return<int()>::value, "");

int main() {}
//]
