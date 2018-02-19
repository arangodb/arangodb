/*<-
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/

//[ function_types_remove_const_comparison
#include <type_traits>
#include <boost/callable_traits/remove_member_const.hpp>

struct foo {
        void bar() const {}
};

using const_removed = boost::callable_traits::remove_member_const_t<decltype(&foo::bar)>;

static_assert(std::is_same<const_removed, void(foo::*)()>::value, "");

int main(){}

//]
