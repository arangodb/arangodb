/*<-
Copyright (c) 2016 Barrett Adair

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

    test_case<int(),                               int(foo::*)()                               >();

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    test_case<int() TX_SAFE,                       int(foo::*)() TX_SAFE                       >();
    test_case<int() LREF,                          int(foo::*)() LREF                          >();
    test_case<int() LREF TX_SAFE,                  int(foo::*)() LREF TX_SAFE                  >();
    test_case<int() RREF,                          int(foo::*)() RREF                          >();
    test_case<int() RREF TX_SAFE,                  int(foo::*)() RREF TX_SAFE                  >();
    test_case<int() const,                         int(foo::*)() const                         >();
    test_case<int() const TX_SAFE,                 int(foo::*)() const TX_SAFE                 >();
    test_case<int() const LREF,                    int(foo::*)() const LREF                    >();
    test_case<int() const LREF TX_SAFE,            int(foo::*)() const LREF TX_SAFE            >();
    test_case<int() const RREF,                    int(foo::*)() const RREF                    >();
    test_case<int() const RREF TX_SAFE,            int(foo::*)() const RREF TX_SAFE            >();
    test_case<int() volatile,                      int(foo::*)() volatile                      >();
    test_case<int() volatile TX_SAFE,              int(foo::*)() volatile TX_SAFE              >();
    test_case<int() volatile LREF,                 int(foo::*)() volatile LREF                 >();
    test_case<int() volatile LREF TX_SAFE,         int(foo::*)() volatile LREF TX_SAFE         >();
    test_case<int() volatile RREF,                 int(foo::*)() volatile RREF                 >();
    test_case<int() volatile RREF TX_SAFE,         int(foo::*)() volatile RREF TX_SAFE         >();
#endif

    test_case<int(int),                            int(foo::*)(int)                            >();

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    test_case<int(int) TX_SAFE,                    int(foo::*)(int) TX_SAFE                    >();
    test_case<int(int) LREF,                       int(foo::*)(int) LREF                       >();
    test_case<int(int) LREF TX_SAFE,               int(foo::*)(int) LREF TX_SAFE               >();
    test_case<int(int) RREF,                       int(foo::*)(int) RREF                       >();
    test_case<int(int) RREF TX_SAFE,               int(foo::*)(int) RREF TX_SAFE               >();
    test_case<int(int) const,                      int(foo::*)(int) const                      >();
    test_case<int(int) const TX_SAFE,              int(foo::*)(int) const TX_SAFE              >();
    test_case<int(int) const LREF,                 int(foo::*)(int) const LREF                 >();
    test_case<int(int) const LREF TX_SAFE,         int(foo::*)(int) const LREF TX_SAFE         >();
    test_case<int(int) const RREF,                 int(foo::*)(int) const RREF                 >();
    test_case<int(int) const RREF TX_SAFE,         int(foo::*)(int) const RREF TX_SAFE         >();
    test_case<int(int) volatile,                   int(foo::*)(int) volatile                   >();
    test_case<int(int) volatile TX_SAFE,           int(foo::*)(int) volatile TX_SAFE           >();
    test_case<int(int) volatile LREF,              int(foo::*)(int) volatile LREF              >();
    test_case<int(int) volatile LREF TX_SAFE,      int(foo::*)(int) volatile LREF TX_SAFE      >();
    test_case<int(int) volatile RREF,              int(foo::*)(int) volatile RREF              >();
    test_case<int(int) volatile RREF TX_SAFE,      int(foo::*)(int) volatile RREF TX_SAFE      >();
#endif

//MSVC doesn't like varargs on abominable functions
#ifndef BOOST_CLBL_TRTS_MSVC

    test_case<int(...),                            int(foo::*)(...)                            >();


#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    test_case<int(...) TX_SAFE,                    int(foo::*)(...) TX_SAFE                    >();
    test_case<int(...) LREF,                       int(foo::*)(...) LREF                       >();
    test_case<int(...) LREF TX_SAFE,               int(foo::*)(...) LREF TX_SAFE               >();
    test_case<int(...) RREF,                       int(foo::*)(...) RREF                       >();
    test_case<int(...) RREF TX_SAFE,               int(foo::*)(...) RREF TX_SAFE               >();
    test_case<int(...) const,                      int(foo::*)(...) const                      >();
    test_case<int(...) const TX_SAFE,              int(foo::*)(...) const TX_SAFE              >();
    test_case<int(...) const LREF,                 int(foo::*)(...) const LREF                 >();
    test_case<int(...) const LREF TX_SAFE,         int(foo::*)(...) const LREF TX_SAFE         >();
    test_case<int(...) const RREF,                 int(foo::*)(...) const RREF                 >();
    test_case<int(...) const RREF TX_SAFE,         int(foo::*)(...) const RREF TX_SAFE         >();
    test_case<int(...) volatile,                   int(foo::*)(...) volatile                   >();
    test_case<int(...) volatile TX_SAFE,           int(foo::*)(...) volatile TX_SAFE           >();
    test_case<int(...) volatile LREF,              int(foo::*)(...) volatile LREF              >();
    test_case<int(...) volatile LREF TX_SAFE,      int(foo::*)(...) volatile LREF TX_SAFE      >();
    test_case<int(...) volatile RREF,              int(foo::*)(...) volatile RREF              >();
    test_case<int(...) volatile RREF TX_SAFE,      int(foo::*)(...) volatile RREF TX_SAFE      >();
#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    test_case<int(int, ...),                       int(foo::*)(int, ...)                       >();

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    test_case<int(int, ...) TX_SAFE,               int(foo::*)(int, ...) TX_SAFE               >();
    test_case<int(int, ...) LREF,                  int(foo::*)(int, ...) LREF                  >();
    test_case<int(int, ...) LREF TX_SAFE,          int(foo::*)(int, ...) LREF TX_SAFE          >();
    test_case<int(int, ...) RREF,                  int(foo::*)(int, ...) RREF                  >();
    test_case<int(int, ...) RREF TX_SAFE,          int(foo::*)(int, ...) RREF TX_SAFE          >();
    test_case<int(int, ...) const,                 int(foo::*)(int, ...) const                 >();
    test_case<int(int, ...) const TX_SAFE,         int(foo::*)(int, ...) const TX_SAFE         >();
    test_case<int(int, ...) const LREF,            int(foo::*)(int, ...) const LREF            >();
    test_case<int(int, ...) const LREF TX_SAFE,    int(foo::*)(int, ...) const LREF TX_SAFE    >();
    test_case<int(int, ...) const RREF,            int(foo::*)(int, ...) const RREF            >();
    test_case<int(int, ...) const RREF TX_SAFE,    int(foo::*)(int, ...) const RREF TX_SAFE    >();
    test_case<int(int, ...) volatile,              int(foo::*)(int, ...) volatile              >();
    test_case<int(int, ...) volatile TX_SAFE,      int(foo::*)(int, ...) volatile TX_SAFE      >();
    test_case<int(int, ...) volatile LREF,         int(foo::*)(int, ...) volatile LREF         >();
    test_case<int(int, ...) volatile LREF TX_SAFE, int(foo::*)(int, ...) volatile LREF TX_SAFE >();
    test_case<int(int, ...) volatile RREF,         int(foo::*)(int, ...) volatile RREF         >();
    test_case<int(int, ...) volatile RREF TX_SAFE, int(foo::*)(int, ...) volatile RREF TX_SAFE >();
#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
#endif //#ifndef BOOST_CLBL_TRTS_MSVC

}
