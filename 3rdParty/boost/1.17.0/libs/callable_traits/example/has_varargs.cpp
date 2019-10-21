/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#ifdef BOOST_CLBL_TRTS_MSVC
// MSVC requires __cdecl for varargs, and I don't want to clutter the example
int main(){}
#else
    
//[ has_varargs
#include <type_traits>
#include <boost/callable_traits/has_varargs.hpp>

namespace ct = boost::callable_traits;

static_assert(ct::has_varargs<int(...)>::value, "");
static_assert(!ct::has_varargs<int()>::value, "");

int main() {}
//]
#endif
