/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>

#ifndef BOOST_CLBL_TRTS_ENABLE_NOEXCEPT_TYPES
int main(){}
#else

//[ add_noexcept
#include <type_traits>
#include <boost/callable_traits/add_noexcept.hpp>

using boost::callable_traits::add_noexcept_t;

static_assert(std::is_same<
    add_noexcept_t<int()>,
    int() noexcept
>{}, "");

int main() {}

//]
#endif

