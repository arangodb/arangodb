/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>

#ifndef BOOST_CLBL_TRTS_ENABLE_NOEXCEPT_TYPES
int main(){}
#else

//[ is_noexcept
#include <boost/callable_traits/is_noexcept.hpp>

namespace ct = boost::callable_traits;

struct foo;

static_assert(ct::is_noexcept<int() noexcept>::value, "");
static_assert(ct::is_noexcept<int(*)() noexcept>::value, "");
static_assert(ct::is_noexcept<int(&)() noexcept>::value, "");
static_assert(ct::is_noexcept<int(foo::*)() const noexcept>::value, "");

static_assert(!ct::is_noexcept<int()>::value, "");
static_assert(!ct::is_noexcept<int(*)()>::value, "");
static_assert(!ct::is_noexcept<int(&)()>::value, "");
static_assert(!ct::is_noexcept<int(foo::*)() const>::value, "");

int main() {}
//]
#endif
