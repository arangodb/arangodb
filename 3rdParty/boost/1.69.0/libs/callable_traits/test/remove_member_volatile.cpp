/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <tuple>
#include <boost/callable_traits/remove_member_volatile.hpp>
#include "test.hpp"

struct foo{};

int main() {
    
    {
        using f   = char(foo::*)(foo*, int);
        using l   = char(foo::*)(foo*, int) LREF;
        using r   = char(foo::*)(foo*, int) RREF;
        using c   = char(foo::*)(foo*, int) const;
        using cl  = char(foo::*)(foo*, int) const LREF;
        using cr  = char(foo::*)(foo*, int) const RREF;
        using v   = char(foo::*)(foo*, int) volatile;
        using vl  = char(foo::*)(foo*, int) volatile LREF;
        using vr  = char(foo::*)(foo*, int) volatile RREF;
        using cv  = char(foo::*)(foo*, int) const volatile;
        using cvl = char(foo::*)(foo*, int) const volatile LREF;
        using cvr = char(foo::*)(foo*, int) const volatile RREF;

        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_volatile, f)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_volatile, vl)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_volatile, l)>{});
        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_volatile, v)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_volatile, r)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_volatile, vr)>{});
        CT_ASSERT(std::is_same<c,   TRAIT(remove_member_volatile, c)>{});
        CT_ASSERT(std::is_same<c,   TRAIT(remove_member_volatile, cv)>{});
        CT_ASSERT(std::is_same<cl,  TRAIT(remove_member_volatile, cl)>{});
        CT_ASSERT(std::is_same<cl,  TRAIT(remove_member_volatile, cvl)>{});
        CT_ASSERT(std::is_same<cr,  TRAIT(remove_member_volatile, cr)>{});
        CT_ASSERT(std::is_same<cr,  TRAIT(remove_member_volatile, cvr)>{});
    }
    
#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    {
        using f   = foo&&();
        using l   = foo&&() LREF;
        using r   = foo&&() RREF;
        using c   = foo&&() const;
        using cl  = foo&&() const LREF;
        using cr  = foo&&() const RREF;
        using v   = foo&&() volatile;
        using vl  = foo&&() volatile LREF;
        using vr  = foo&&() volatile RREF;
        using cv  = foo&&() const volatile;
        using cvl = foo&&() const volatile LREF;
        using cvr = foo&&() const volatile RREF;

        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_volatile, f)>{});
        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_volatile, v)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_volatile, vl)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_volatile, l)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_volatile, r)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_volatile, vr)>{});
        CT_ASSERT(std::is_same<c,   TRAIT(remove_member_volatile, c)>{});
        CT_ASSERT(std::is_same<c,   TRAIT(remove_member_volatile, cv)>{});
        CT_ASSERT(std::is_same<cl,  TRAIT(remove_member_volatile, cl)>{});
        CT_ASSERT(std::is_same<cl,  TRAIT(remove_member_volatile, cvl)>{});
        CT_ASSERT(std::is_same<cr,  TRAIT(remove_member_volatile, cr)>{});
        CT_ASSERT(std::is_same<cr,  TRAIT(remove_member_volatile, cvr)>{});
    }

#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

}
