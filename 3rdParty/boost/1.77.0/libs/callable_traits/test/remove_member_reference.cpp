/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <tuple>
#include <boost/callable_traits/remove_member_reference.hpp>
#include "test.hpp"

#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main(){ return 0; }
#else


struct foo{};

int main() {
    {
        using f   = void(foo::*)();
        using l   = void(foo::*)() &;
        using r   = void(foo::*)() &&;
        using c   = void(foo::*)() const;
        using cl  = void(foo::*)() const &;
        using cr  = void(foo::*)() const &&;
        using v   = void(foo::*)() volatile;
        using vl  = void(foo::*)() volatile &;
        using vr  = void(foo::*)() volatile &&;
        using cv  = void(foo::*)() const volatile;
        using cvl = void(foo::*)() const volatile &;
        using cvr = void(foo::*)() const volatile &&;

        CT_ASSERT(std::is_same<f,    TRAIT(remove_member_reference,  f)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(remove_member_reference,  c)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(remove_member_reference,  v)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(remove_member_reference,  cv)>{});
        CT_ASSERT(std::is_same<f,    TRAIT(remove_member_reference,  l)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(remove_member_reference,  cl)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(remove_member_reference,  vl)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(remove_member_reference,  cvl)>{});
        CT_ASSERT(std::is_same<f,    TRAIT(remove_member_reference,  r)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(remove_member_reference,  cr)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(remove_member_reference,  vr)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(remove_member_reference,  cvr)>{});
    }

    {
        using f   = void();
        using l   = void() &;
        using r   = void() &&;
        using c   = void() const;
        using cl  = void() const &;
        using cr  = void() const &&;
        using v   = void() volatile;
        using vl  = void() volatile &;
        using vr  = void() volatile &&;
        using cv  = void() const volatile;
        using cvl = void() const volatile &;
        using cvr = void() const volatile &&;

        CT_ASSERT(std::is_same<f,    TRAIT(remove_member_reference,  f)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(remove_member_reference,  c)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(remove_member_reference,  v)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(remove_member_reference,  cv)>{});
        CT_ASSERT(std::is_same<f,    TRAIT(remove_member_reference,  l)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(remove_member_reference,  cl)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(remove_member_reference,  vl)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(remove_member_reference,  cvl)>{});
        CT_ASSERT(std::is_same<f,    TRAIT(remove_member_reference,  r)>{});
        CT_ASSERT(std::is_same<c,    TRAIT(remove_member_reference,  cr)>{});
        CT_ASSERT(std::is_same<v,    TRAIT(remove_member_reference,  vr)>{});
        CT_ASSERT(std::is_same<cv,   TRAIT(remove_member_reference,  cvr)>{});
    }
}

#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
