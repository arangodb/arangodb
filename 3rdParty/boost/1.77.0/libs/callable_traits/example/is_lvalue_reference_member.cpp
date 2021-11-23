/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>

#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main(){ return 0; }
#else

//[ is_lvalue_reference_member
#include <type_traits>
#include <boost/callable_traits/is_lvalue_reference_member.hpp>

namespace ct = boost::callable_traits;

static_assert(ct::is_lvalue_reference_member<int()&>::value, "");
static_assert(!ct::is_lvalue_reference_member<int()&&>::value, "");
static_assert(!ct::is_lvalue_reference_member<int()>::value, "");

struct foo;

static_assert(ct::is_lvalue_reference_member<int(foo::*)()&>::value, "");
static_assert(!ct::is_lvalue_reference_member<int(foo::*)()&&>::value, "");
static_assert(!ct::is_lvalue_reference_member<int(foo::*)()>::value, "");

int main() {}
//]
#endif
