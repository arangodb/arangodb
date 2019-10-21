/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

//[ has_member_qualifiers
#include <type_traits>
#include <boost/callable_traits/has_member_qualifiers.hpp>

namespace ct = boost::callable_traits;

struct foo;

static_assert(ct::has_member_qualifiers<int(foo::*)() const>::value, "");
static_assert(ct::has_member_qualifiers<int(foo::*)() volatile>::value, "");
static_assert(!ct::has_member_qualifiers<int(foo::*)()>::value, "");

int main() {}
//]
