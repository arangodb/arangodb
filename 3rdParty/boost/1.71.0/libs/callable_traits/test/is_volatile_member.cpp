/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <utility>
#include <boost/callable_traits/is_volatile_member.hpp>
#include "test.hpp"

struct foo {};

template<typename T>
void assert_volatile_qualified() {
    CT_ASSERT( is_volatile_member<T>::value);
}


template<typename T>
void assert_not_volatile_qualified() {
    CT_ASSERT(! is_volatile_member<T>::value);
}

int main() {

    {
        using f   = void(foo::*)();
        using l   = void(foo::*)() LREF;
        using r   = void(foo::*)() RREF ;
        using c   = void(foo::*)() const;
        using cl  = void(foo::*)() const LREF;
        using cr  = void(foo::*)() const RREF;
        using v   = void(foo::*)() volatile;
        using vl  = void(foo::*)() volatile LREF;
        using vr  = void(foo::*)() volatile RREF;
        using cv  = void(foo::*)() const volatile;
        using cvl = void(foo::*)() const volatile LREF;
        using cvr = void(foo::*)() const volatile RREF;

        assert_not_volatile_qualified<f>();
        assert_not_volatile_qualified<l>();
        assert_not_volatile_qualified<r>();
        assert_not_volatile_qualified<c>();
        assert_not_volatile_qualified<cl>();
        assert_not_volatile_qualified<cr>();
        assert_volatile_qualified<v>();
        assert_volatile_qualified<vl>();
        assert_volatile_qualified<vr>();
        assert_volatile_qualified<cv>();
        assert_volatile_qualified<cvl>();
        assert_volatile_qualified<cvr>();
    }

    {
        struct f   { int operator()() { return 0; } };
        struct l   { int operator()() LREF { return 0; } };
        struct r   { int operator()() RREF { return 0; } };
        struct c   { int operator()() const { return 0; } };
        struct cl  { int operator()() const LREF { return 0; } };
        struct cr  { int operator()() const RREF { return 0; } };
        struct v   { int operator()() volatile { return 0; } };
        struct vl  { int operator()() volatile LREF { return 0; } };
        struct vr  { int operator()() volatile RREF { return 0; } };
        struct cv  { int operator()() const volatile { return 0; } };
        struct cvl { int operator()() const volatile LREF { return 0; } };
        struct cvr { int operator()() const volatile RREF { return 0; } };

        assert_not_volatile_qualified<f>();
        assert_not_volatile_qualified<l>();
        assert_not_volatile_qualified<r>();
        assert_not_volatile_qualified<c>();
        assert_not_volatile_qualified<cl>();
        assert_not_volatile_qualified<cr>();
        assert_volatile_qualified<v>();
        assert_volatile_qualified<vl>();
        assert_volatile_qualified<vr>();
        assert_volatile_qualified<cv>();
        assert_volatile_qualified<cvl>();
        assert_volatile_qualified<cvr>();
    }

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    {
        using f   = void();
        using l   = void() LREF;
        using r   = void() RREF ;
        using c   = void() const;
        using cl  = void() const LREF;
        using cr  = void() const RREF;
        using v   = void() volatile;
        using vl  = void() volatile LREF;
        using vr  = void() volatile RREF;
        using cv  = void() const volatile;
        using cvl = void() const volatile LREF;
        using cvr = void() const volatile RREF;

        CT_ASSERT(! is_volatile_member<f>());
        CT_ASSERT(! is_volatile_member<l>());
        CT_ASSERT(! is_volatile_member<r>());
        CT_ASSERT(! is_volatile_member<c>());
        CT_ASSERT(! is_volatile_member<cl>());
        CT_ASSERT(! is_volatile_member<cr>());
        CT_ASSERT( is_volatile_member<v>());
        CT_ASSERT( is_volatile_member<vl>());
        CT_ASSERT( is_volatile_member<vr>());
        CT_ASSERT( is_volatile_member<cv>());
        CT_ASSERT( is_volatile_member<cvl>());
        CT_ASSERT( is_volatile_member<cvr>());
    }

#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    using f_ptr = void(*)();
    assert_not_volatile_qualified<f_ptr>();
    assert_not_volatile_qualified<f_ptr foo::*>();
    assert_not_volatile_qualified<int foo::*>();
    assert_not_volatile_qualified<void(&)()>();
}
