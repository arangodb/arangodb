/*<-
Copyright (c) 2016 arett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/


#include <boost/callable_traits/apply_member_pointer.hpp>
#include "test.hpp"

struct a;
struct b;

template<typename Input, typename Output>
void test_case() {
    assert_same< TRAIT(apply_member_pointer, Input, b), Output>();
}

int main() {

    test_case<int(a::*)(),                               int(b::*)()                               >();
    test_case<int(a::*)() TX_SAFE,                       int(b::*)() TX_SAFE                       >();
    test_case<int(a::*)() LREF,                          int(b::*)() LREF                          >();
    test_case<int(a::*)() LREF TX_SAFE,                  int(b::*)() LREF TX_SAFE                  >();
    test_case<int(a::*)() RREF,                          int(b::*)() RREF                          >();
    test_case<int(a::*)() RREF TX_SAFE,                  int(b::*)() RREF TX_SAFE                  >();
    test_case<int(a::*)() const,                         int(b::*)() const                         >();
    test_case<int(a::*)() const TX_SAFE,                 int(b::*)() const TX_SAFE                 >();
    test_case<int(a::*)() const LREF,                    int(b::*)() const LREF                    >();
    test_case<int(a::*)() const LREF TX_SAFE,            int(b::*)() const LREF TX_SAFE            >();
    test_case<int(a::*)() const RREF,                    int(b::*)() const RREF                    >();
    test_case<int(a::*)() const RREF TX_SAFE,            int(b::*)() const RREF TX_SAFE            >();
    test_case<int(a::*)() volatile,                      int(b::*)() volatile                      >();
    test_case<int(a::*)() volatile TX_SAFE,              int(b::*)() volatile TX_SAFE              >();
    test_case<int(a::*)() volatile LREF,                 int(b::*)() volatile LREF                 >();
    test_case<int(a::*)() volatile LREF TX_SAFE,         int(b::*)() volatile LREF TX_SAFE         >();
    test_case<int(a::*)() volatile RREF,                 int(b::*)() volatile RREF                 >();
    test_case<int(a::*)() volatile RREF TX_SAFE,         int(b::*)() volatile RREF TX_SAFE         >();
    test_case<int(a::*)(int),                            int(b::*)(int)                            >();
    test_case<int(a::*)(int) TX_SAFE,                    int(b::*)(int) TX_SAFE                    >();
    test_case<int(a::*)(int) LREF,                       int(b::*)(int) LREF                       >();
    test_case<int(a::*)(int) LREF TX_SAFE,               int(b::*)(int) LREF TX_SAFE               >();
    test_case<int(a::*)(int) RREF,                       int(b::*)(int) RREF                       >();
    test_case<int(a::*)(int) RREF TX_SAFE,               int(b::*)(int) RREF TX_SAFE               >();
    test_case<int(a::*)(int) const,                      int(b::*)(int) const                      >();
    test_case<int(a::*)(int) const TX_SAFE,              int(b::*)(int) const TX_SAFE              >();
    test_case<int(a::*)(int) const LREF,                 int(b::*)(int) const LREF                 >();
    test_case<int(a::*)(int) const LREF TX_SAFE,         int(b::*)(int) const LREF TX_SAFE         >();
    test_case<int(a::*)(int) const RREF,                 int(b::*)(int) const RREF                 >();
    test_case<int(a::*)(int) const RREF TX_SAFE,         int(b::*)(int) const RREF TX_SAFE         >();
    test_case<int(a::*)(int) volatile,                   int(b::*)(int) volatile                   >();
    test_case<int(a::*)(int) volatile TX_SAFE,           int(b::*)(int) volatile TX_SAFE           >();
    test_case<int(a::*)(int) volatile LREF,              int(b::*)(int) volatile LREF              >();
    test_case<int(a::*)(int) volatile LREF TX_SAFE,      int(b::*)(int) volatile LREF TX_SAFE      >();
    test_case<int(a::*)(int) volatile RREF,              int(b::*)(int) volatile RREF              >();
    test_case<int(a::*)(int) volatile RREF TX_SAFE,      int(b::*)(int) volatile RREF TX_SAFE      >();

    test_case<int(VA_CC a::*)(...),                            int(VA_CC b::*)(...)                            >();
    test_case<int(VA_CC a::*)(...) TX_SAFE,                    int(VA_CC b::*)(...) TX_SAFE                    >();
    test_case<int(VA_CC a::*)(...) LREF,                       int(VA_CC b::*)(...) LREF                       >();
    test_case<int(VA_CC a::*)(...) LREF TX_SAFE,               int(VA_CC b::*)(...) LREF TX_SAFE               >();
    test_case<int(VA_CC a::*)(...) RREF,                       int(VA_CC b::*)(...) RREF                       >();
    test_case<int(VA_CC a::*)(...) RREF TX_SAFE,               int(VA_CC b::*)(...) RREF TX_SAFE               >();
    test_case<int(VA_CC a::*)(...) const,                      int(VA_CC b::*)(...) const                      >();
    test_case<int(VA_CC a::*)(...) const TX_SAFE,              int(VA_CC b::*)(...) const TX_SAFE              >();
    test_case<int(VA_CC a::*)(...) const LREF,                 int(VA_CC b::*)(...) const LREF                 >();
    test_case<int(VA_CC a::*)(...) const LREF TX_SAFE,         int(VA_CC b::*)(...) const LREF TX_SAFE         >();
    test_case<int(VA_CC a::*)(...) const RREF,                 int(VA_CC b::*)(...) const RREF                 >();
    test_case<int(VA_CC a::*)(...) const RREF TX_SAFE,         int(VA_CC b::*)(...) const RREF TX_SAFE         >();
    test_case<int(VA_CC a::*)(...) volatile,                   int(VA_CC b::*)(...) volatile                   >();
    test_case<int(VA_CC a::*)(...) volatile TX_SAFE,           int(VA_CC b::*)(...) volatile TX_SAFE           >();
    test_case<int(VA_CC a::*)(...) volatile LREF,              int(VA_CC b::*)(...) volatile LREF              >();
    test_case<int(VA_CC a::*)(...) volatile LREF TX_SAFE,      int(VA_CC b::*)(...) volatile LREF TX_SAFE      >();
    test_case<int(VA_CC a::*)(...) volatile RREF,              int(VA_CC b::*)(...) volatile RREF              >();
    test_case<int(VA_CC a::*)(...) volatile RREF TX_SAFE,      int(VA_CC b::*)(...) volatile RREF TX_SAFE      >();
    test_case<int(VA_CC a::*)(int, ...),                       int(VA_CC b::*)(int, ...)                       >();
    test_case<int(VA_CC a::*)(int, ...) TX_SAFE,               int(VA_CC b::*)(int, ...) TX_SAFE               >();
    test_case<int(VA_CC a::*)(int, ...) LREF,                  int(VA_CC b::*)(int, ...) LREF                  >();
    test_case<int(VA_CC a::*)(int, ...) LREF TX_SAFE,          int(VA_CC b::*)(int, ...) LREF TX_SAFE          >();
    test_case<int(VA_CC a::*)(int, ...) RREF,                  int(VA_CC b::*)(int, ...) RREF                  >();
    test_case<int(VA_CC a::*)(int, ...) RREF TX_SAFE,          int(VA_CC b::*)(int, ...) RREF TX_SAFE          >();
    test_case<int(VA_CC a::*)(int, ...) const,                 int(VA_CC b::*)(int, ...) const                 >();
    test_case<int(VA_CC a::*)(int, ...) const TX_SAFE,         int(VA_CC b::*)(int, ...) const TX_SAFE         >();
    test_case<int(VA_CC a::*)(int, ...) const LREF,            int(VA_CC b::*)(int, ...) const LREF            >();
    test_case<int(VA_CC a::*)(int, ...) const LREF TX_SAFE,    int(VA_CC b::*)(int, ...) const LREF TX_SAFE    >();
    test_case<int(VA_CC a::*)(int, ...) const RREF,            int(VA_CC b::*)(int, ...) const RREF            >();
    test_case<int(VA_CC a::*)(int, ...) const RREF TX_SAFE,    int(VA_CC b::*)(int, ...) const RREF TX_SAFE    >();
    test_case<int(VA_CC a::*)(int, ...) volatile,              int(VA_CC b::*)(int, ...) volatile              >();
    test_case<int(VA_CC a::*)(int, ...) volatile TX_SAFE,      int(VA_CC b::*)(int, ...) volatile TX_SAFE      >();
    test_case<int(VA_CC a::*)(int, ...) volatile LREF,         int(VA_CC b::*)(int, ...) volatile LREF         >();
    test_case<int(VA_CC a::*)(int, ...) volatile LREF TX_SAFE, int(VA_CC b::*)(int, ...) volatile LREF TX_SAFE >();
    test_case<int(VA_CC a::*)(int, ...) volatile RREF,         int(VA_CC b::*)(int, ...) volatile RREF         >();
    test_case<int(VA_CC a::*)(int, ...) volatile RREF TX_SAFE, int(VA_CC b::*)(int, ...) volatile RREF TX_SAFE >();
}
