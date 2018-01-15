/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <tuple>
#include <boost/callable_traits/add_member_const.hpp>
#include "test.hpp"

struct foo {};

int main() {

    {
        using f =   void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...);
        using l =   void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) LREF;
        using r =   void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) RREF ;
        using c =   void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const;
        using cl =  void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const LREF;
        using cr =  void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const RREF;
        using v =   void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile;
        using vl =  void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile LREF;
        using vr =  void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile RREF;
        using cv =  void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile;
        using cvl = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile LREF;
        using cvr = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile RREF;

        CT_ASSERT(std::is_same<c,    TRAIT(add_member_const, f)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(add_member_const, c)>{});
        CT_ASSERT(std::is_same<cl,   TRAIT(add_member_const, l)>{});
        CT_ASSERT(std::is_same<cl,   TRAIT(add_member_const, cl)>{});
        CT_ASSERT(std::is_same<cr,   TRAIT(add_member_const, r)>{});
        CT_ASSERT(std::is_same<cr,   TRAIT(add_member_const, cr)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_const, v)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_const, cv)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_const, vl)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_const, cvl)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_const, vr)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_const, cvr)>{});
    }

    {
        using f =   void(foo::*)(int, int);
        using l =   void(foo::*)(int, int) LREF;
        using r =   void(foo::*)(int, int) RREF ;
        using c =   void(foo::*)(int, int) const;
        using cl =  void(foo::*)(int, int) const LREF;
        using cr =  void(foo::*)(int, int) const RREF;
        using v =   void(foo::*)(int, int) volatile;
        using vl =  void(foo::*)(int, int) volatile LREF;
        using vr =  void(foo::*)(int, int) volatile RREF;
        using cv =  void(foo::*)(int, int) const volatile;
        using cvl = void(foo::*)(int, int) const volatile LREF;
        using cvr = void(foo::*)(int, int) const volatile RREF;

        CT_ASSERT(std::is_same<c,    TRAIT(add_member_const, f)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(add_member_const, c)>{});
        CT_ASSERT(std::is_same<cl,   TRAIT(add_member_const, l)>{});
        CT_ASSERT(std::is_same<cl,   TRAIT(add_member_const, cl)>{});
        CT_ASSERT(std::is_same<cr,   TRAIT(add_member_const, r)>{});
        CT_ASSERT(std::is_same<cr,   TRAIT(add_member_const, cr)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_const, v)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_const, cv)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_const, vl)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_const, cvl)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_const, vr)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_const, cvr)>{});
    }

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    {
        using f =   void();
        using l =   void() LREF;
        using r =   void() RREF ;
        using c =   void() const;
        using cl =  void() const LREF;
        using cr =  void() const RREF;
        using v =   void() volatile;
        using vl =  void() volatile LREF;
        using vr =  void() volatile RREF;
        using cv =  void() const volatile;
        using cvl = void() const volatile LREF;
        using cvr = void() const volatile RREF;

        CT_ASSERT(std::is_same<c,    TRAIT(add_member_const, f)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(add_member_const, c)>{});
        CT_ASSERT(std::is_same<cl,   TRAIT(add_member_const, l)>{});
        CT_ASSERT(std::is_same<cl,   TRAIT(add_member_const, cl)>{});
        CT_ASSERT(std::is_same<cr,   TRAIT(add_member_const, r)>{});
        CT_ASSERT(std::is_same<cr,   TRAIT(add_member_const, cr)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_const, v)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(add_member_const, cv)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_const, vl)>{});
        CT_ASSERT(std::is_same<cvl,  TRAIT(add_member_const, cvl)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_const, vr)>{});
        CT_ASSERT(std::is_same<cvr,  TRAIT(add_member_const, cvr)>{});
    }
#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
}
