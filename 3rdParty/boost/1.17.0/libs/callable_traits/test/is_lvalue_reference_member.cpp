/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <utility>
#include <boost/callable_traits/is_lvalue_reference_member.hpp>
#include "test.hpp"


#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main(){ return 0; }
#else

struct foo {};

template<typename T>
void assert_lvalue_qualified() {
    
    CT_ASSERT( is_lvalue_reference_member<T>());
}


template<typename T>
void assert_not_lvalue_qualified() {

    CT_ASSERT(! is_lvalue_reference_member<T>());
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

        assert_not_lvalue_qualified<f>();
        assert_lvalue_qualified<l>();
        assert_not_lvalue_qualified<r>();
        assert_not_lvalue_qualified<c>();
        assert_lvalue_qualified<cl>();
        assert_not_lvalue_qualified<cr>();
        assert_not_lvalue_qualified<v>();
        assert_lvalue_qualified<vl>();
        assert_not_lvalue_qualified<vr>();
        assert_not_lvalue_qualified<cv>();
        assert_lvalue_qualified<cvl>();
        assert_not_lvalue_qualified<cvr>();
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

        assert_not_lvalue_qualified<f>();
        assert_lvalue_qualified<l>();
        assert_not_lvalue_qualified<r>();
        assert_not_lvalue_qualified<c>();
        assert_lvalue_qualified<cl>();
        assert_not_lvalue_qualified<cr>();
        assert_not_lvalue_qualified<v>();
        assert_lvalue_qualified<vl>();
        assert_not_lvalue_qualified<vr>();
        assert_not_lvalue_qualified<cv>();
        assert_lvalue_qualified<cvl>();
        assert_not_lvalue_qualified<cvr>();
    }

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

        CT_ASSERT(! is_lvalue_reference_member<f>());
        CT_ASSERT( is_lvalue_reference_member<l>());
        CT_ASSERT(! is_lvalue_reference_member<r>());
        CT_ASSERT(! is_lvalue_reference_member<c>());
        CT_ASSERT( is_lvalue_reference_member<cl>());
        CT_ASSERT(! is_lvalue_reference_member<cr>());
        CT_ASSERT(! is_lvalue_reference_member<v>());
        CT_ASSERT( is_lvalue_reference_member<vl>());
        CT_ASSERT(! is_lvalue_reference_member<vr>());
        CT_ASSERT(! is_lvalue_reference_member<cv>());
        CT_ASSERT( is_lvalue_reference_member<cvl>());
        CT_ASSERT(! is_lvalue_reference_member<cvr>());
    }

    using f_ptr = void(*)();
    assert_not_lvalue_qualified<f_ptr>();
    assert_not_lvalue_qualified<f_ptr foo::*>();
    assert_not_lvalue_qualified<int foo::*>();
    assert_not_lvalue_qualified<void(&)()>();
}

#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
