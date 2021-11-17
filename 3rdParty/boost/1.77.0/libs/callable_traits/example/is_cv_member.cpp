/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

//[ is_cv_member
#include <type_traits>
#include <boost/callable_traits/is_cv_member.hpp>

namespace ct = boost::callable_traits;

struct foo;

static_assert(ct::is_cv_member<int(foo::*)() const volatile>::value, "");
static_assert(!ct::is_cv_member<int(foo::*)()>::value, "");
static_assert(!ct::is_cv_member<int(foo::*)() const>::value, "");
static_assert(!ct::is_cv_member<int(foo::*)() volatile>::value, "");

int main() {}
//]
