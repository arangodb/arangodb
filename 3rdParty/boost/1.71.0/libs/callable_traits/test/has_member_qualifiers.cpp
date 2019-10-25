/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <utility>
#include <boost/callable_traits/has_member_qualifiers.hpp>
#include "test.hpp"


#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main() { return 0; }
#else

struct foo {};

template<typename T>
void assert_qualified() {
    CT_ASSERT( has_member_qualifiers<T>::value);
}


template<typename T>
void assert_unqualified() {
    CT_ASSERT(! has_member_qualifiers<T>::value);
}

int main() {

    {
        using f   = void(foo::*)();
        using l   = void(foo::*)() &;
        using r   = void(foo::*)() && ;
        using c   = void(foo::*)() const;
        using cl  = void(foo::*)() const &;
        using cr  = void(foo::*)() const &&;
        using v   = void(foo::*)() volatile;
        using vl  = void(foo::*)() volatile &;
        using vr  = void(foo::*)() volatile &&;
        using cv  = void(foo::*)() const volatile;
        using cvl = void(foo::*)() const volatile &;
        using cvr = void(foo::*)() const volatile &&;

        assert_unqualified<f>();
        assert_qualified<l>();
        assert_qualified<r>();
        assert_qualified<c>();
        assert_qualified<cl>();
        assert_qualified<cr>();
        assert_qualified<v>();
        assert_qualified<vl>();
        assert_qualified<vr>();
        assert_qualified<cv>();
        assert_qualified<cvl>();
        assert_qualified<cvr>();
    }

    {
        struct f   { int operator()() { return 0; } };
        struct l   { int operator()() & { return 0; } };
        struct r   { int operator()() && { return 0; } };
        struct c   { int operator()() const { return 0; } };
        struct cl  { int operator()() const & { return 0; } };
        struct cr  { int operator()() const && { return 0; } };
        struct v   { int operator()() volatile { return 0; } };
        struct vl  { int operator()() volatile & { return 0; } };
        struct vr  { int operator()() volatile && { return 0; } };
        struct cv  { int operator()() const volatile { return 0; } };
        struct cvl { int operator()() const volatile & { return 0; } };
        struct cvr { int operator()() const volatile && { return 0; } };

        assert_unqualified<f>();
        assert_qualified<l>();
        assert_qualified<r>();
        assert_qualified<c>();
        assert_qualified<cl>();
        assert_qualified<cr>();
        assert_qualified<v>();
        assert_qualified<vl>();
        assert_qualified<vr>();
        assert_qualified<cv>();
        assert_qualified<cvl>();
        assert_qualified<cvr>();
    }

#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    {
        using f   = void();
        using l   = void() &;
        using r   = void() && ;
        using c   = void() const;
        using cl  = void() const &;
        using cr  = void() const &&;
        using v   = void() volatile;
        using vl  = void() volatile &;
        using vr  = void() volatile &&;
        using cv  = void() const volatile;
        using cvl = void() const volatile &;
        using cvr = void() const volatile &&;

        CT_ASSERT(! has_member_qualifiers<f>());
        CT_ASSERT( has_member_qualifiers<l>());
        CT_ASSERT( has_member_qualifiers<r>());
        CT_ASSERT( has_member_qualifiers<c>());
        CT_ASSERT( has_member_qualifiers<cl>());
        CT_ASSERT( has_member_qualifiers<cr>());
        CT_ASSERT( has_member_qualifiers<v>());
        CT_ASSERT( has_member_qualifiers<vl>());
        CT_ASSERT( has_member_qualifiers<vr>());
        CT_ASSERT( has_member_qualifiers<cv>());
        CT_ASSERT( has_member_qualifiers<cvl>());
        CT_ASSERT( has_member_qualifiers<cvr>());
    }

#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    using f_ptr = void(*)();
    assert_unqualified<f_ptr>();
    assert_unqualified<f_ptr foo::*>();
    assert_unqualified<int foo::*>();
    assert_unqualified<void(&)()>();
}

#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
