/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <tuple>
#include <boost/callable_traits/remove_member_const.hpp>
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

        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_const,  f)>{});
        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_const,  c)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_const,  l)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_const,  cl)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_const,  r)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_const,  cr)>{});
        CT_ASSERT(std::is_same<v,   TRAIT(remove_member_const,  v)>{});
        CT_ASSERT(std::is_same<v,   TRAIT(remove_member_const,  cv)>{});
        CT_ASSERT(std::is_same<vl,  TRAIT(remove_member_const,  vl)>{});
        CT_ASSERT(std::is_same<vl,  TRAIT(remove_member_const,  cvl)>{});
        CT_ASSERT(std::is_same<vr,  TRAIT(remove_member_const,  vr)>{});
        CT_ASSERT(std::is_same<vr,  TRAIT(remove_member_const,  cvr)>{});
    }

    {
        using f =   foo const & (foo::*)(int, int);
        using l =   foo const & (foo::*)(int, int) LREF;
        using r =   foo const & (foo::*)(int, int) RREF ;
        using c =   foo const & (foo::*)(int, int) const;
        using cl =  foo const & (foo::*)(int, int) const LREF;
        using cr =  foo const & (foo::*)(int, int) const RREF;
        using v =   foo const & (foo::*)(int, int) volatile;
        using vl =  foo const & (foo::*)(int, int) volatile LREF;
        using vr =  foo const & (foo::*)(int, int) volatile RREF;
        using cv =  foo const & (foo::*)(int, int) const volatile;
        using cvl = foo const & (foo::*)(int, int) const volatile LREF;
        using cvr = foo const & (foo::*)(int, int) const volatile RREF;

        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_const,  f)>{});
        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_const,  c)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_const,  l)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_const,  cl)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_const,  r)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_const,  cr)>{});
        CT_ASSERT(std::is_same<v,   TRAIT(remove_member_const,  v)>{});
        CT_ASSERT(std::is_same<v,   TRAIT(remove_member_const,  cv)>{});
        CT_ASSERT(std::is_same<vl,  TRAIT(remove_member_const,  vl)>{});
        CT_ASSERT(std::is_same<vl,  TRAIT(remove_member_const,  cvl)>{});
        CT_ASSERT(std::is_same<vr,  TRAIT(remove_member_const,  vr)>{});
        CT_ASSERT(std::is_same<vr,  TRAIT(remove_member_const,  cvr)>{});
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

        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_const,  f)>{});
        CT_ASSERT(std::is_same<f,   TRAIT(remove_member_const,  c)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_const,  l)>{});
        CT_ASSERT(std::is_same<l,   TRAIT(remove_member_const,  cl)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_const,  r)>{});
        CT_ASSERT(std::is_same<r,   TRAIT(remove_member_const,  cr)>{});
        CT_ASSERT(std::is_same<v,   TRAIT(remove_member_const,  v)>{});
        CT_ASSERT(std::is_same<v,   TRAIT(remove_member_const,  cv)>{});
        CT_ASSERT(std::is_same<vl,  TRAIT(remove_member_const,  vl)>{});
        CT_ASSERT(std::is_same<vl,  TRAIT(remove_member_const,  cvl)>{});
        CT_ASSERT(std::is_same<vr,  TRAIT(remove_member_const,  vr)>{});
        CT_ASSERT(std::is_same<vr,  TRAIT(remove_member_const,  cvr)>{});
    }

#endif
}
