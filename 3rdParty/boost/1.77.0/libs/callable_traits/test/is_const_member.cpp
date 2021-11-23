/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <utility>
#include <boost/callable_traits/is_const_member.hpp>
#include "test.hpp"

struct foo {};

template<typename T>
void assert_const_qualified() {
    CT_ASSERT( is_const_member<T>());
}


template<typename T>
void assert_not_const_qualified() {
    CT_ASSERT(! is_const_member<T>());
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

        assert_not_const_qualified<f>();
        assert_not_const_qualified<l>();
        assert_not_const_qualified<r>();
        assert_const_qualified<c>();
        assert_const_qualified<cl>();
        assert_const_qualified<cr>();
        assert_not_const_qualified<v>();
        assert_not_const_qualified<vl>();
        assert_not_const_qualified<vr>();
        assert_const_qualified<cv>();
        assert_const_qualified<cvl>();
        assert_const_qualified<cvr>();
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

        assert_not_const_qualified<f>();
        assert_not_const_qualified<l>();
        assert_not_const_qualified<r>();
        assert_const_qualified<c>();
        assert_const_qualified<cl>();
        assert_const_qualified<cr>();
        assert_not_const_qualified<v>();
        assert_not_const_qualified<vl>();
        assert_not_const_qualified<vr>();
        assert_const_qualified<cv>();
        assert_const_qualified<cvl>();
        assert_const_qualified<cvr>();
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

        CT_ASSERT(! is_const_member<f>());
        CT_ASSERT(! is_const_member<l>());
        CT_ASSERT(! is_const_member<r>());
        CT_ASSERT( is_const_member<c>());
        CT_ASSERT( is_const_member<cl>());
        CT_ASSERT( is_const_member<cr>());
        CT_ASSERT(! is_const_member<v>());
        CT_ASSERT(! is_const_member<vl>());
        CT_ASSERT(! is_const_member<vr>());
        CT_ASSERT( is_const_member<cv>());
        CT_ASSERT( is_const_member<cvl>());
        CT_ASSERT( is_const_member<cvr>());
    }

#endif

    using f_ptr = void(*)();
    assert_not_const_qualified<f_ptr>();
    assert_not_const_qualified<f_ptr foo::*>();
    assert_not_const_qualified<int foo::*>();
    assert_not_const_qualified<void(&)()>();
}
