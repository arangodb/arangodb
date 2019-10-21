/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <tuple>
#include <boost/callable_traits/add_member_volatile.hpp>
#include "test.hpp"

struct foo{};

int main() {
    
    {
        using f   = int(foo::*)(int);
        using l   = int(foo::*)(int) LREF;
        using r   = int(foo::*)(int) RREF;
        using c   = int(foo::*)(int) const;
        using cl  = int(foo::*)(int) const LREF;
        using cr  = int(foo::*)(int) const RREF;
        using v   = int(foo::*)(int) volatile;
        using vl  = int(foo::*)(int) volatile LREF;
        using vr  = int(foo::*)(int) volatile RREF;
        using cv  = int(foo::*)(int) const volatile;
        using cvl = int(foo::*)(int) const volatile LREF;
        using cvr = int(foo::*)(int) const volatile RREF;

        CT_ASSERT(std::is_same<v,    TRAIT(add_member_volatile, f)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(add_member_volatile, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_volatile, l)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_volatile, vl)>{});
        CT_ASSERT(std::is_same<vr,   TRAIT(add_member_volatile, r)>{});
        CT_ASSERT(std::is_same<vr,   TRAIT(add_member_volatile, vr)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_volatile, c)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_volatile, cv)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_volatile, cl)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_volatile, cvl)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_volatile, cr)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_volatile, cvr)>{});
    }
    
#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    {
        using f   = foo();
        using l   = foo() LREF;
        using r   = foo() RREF;
        using c   = foo() const;
        using cl  = foo() const LREF;
        using cr  = foo() const RREF;
        using v   = foo() volatile;
        using vl  = foo() volatile LREF;
        using vr  = foo() volatile RREF;
        using cv  = foo() const volatile;
        using cvl = foo() const volatile LREF;
        using cvr = foo() const volatile RREF;

        CT_ASSERT(std::is_same<v,    TRAIT(add_member_volatile, f)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(add_member_volatile, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_volatile, l)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_volatile, vl)>{});
        CT_ASSERT(std::is_same<vr,   TRAIT(add_member_volatile, r)>{});
        CT_ASSERT(std::is_same<vr,   TRAIT(add_member_volatile, vr)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_volatile, c)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_volatile, cv)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_volatile, cl)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_volatile, cvl)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_volatile, cr)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_volatile, cvr)>{});
    }

#endif
}
