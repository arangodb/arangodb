/*<-
Copyright (c) 2016 arett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/


#include <boost/callable_traits/apply_member_pointer.hpp>
#include "test.hpp"

struct foo;

template<typename Input, typename Output>
void test_case() {
    assert_same<TRAIT(apply_member_pointer, Input, foo), Output>();
}

int main() {
    test_case<int,              int foo::*>();
    test_case<int &,            int foo::*>();
    test_case<int const,        int const foo::*>();
    test_case<int const &,      int const foo::*>();
    test_case<int volatile,     int volatile foo::*>();
    test_case<int volatile &,   int volatile foo::*>();

    //member data - function pointer
    test_case<int(* const)(),   int(* const foo::*)()>();
    test_case<int(* const &)(), int(* const foo::*)()>();
}

