/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <utility>
#include <boost/callable_traits/has_varargs.hpp>
#include "test.hpp"

struct foo {};

template<typename T>
void assert_has_varargs() {

    CT_ASSERT( has_varargs<T>::value);
}

template<typename T>
void assert_not_has_varargs() {

    CT_ASSERT(! has_varargs<T>::value);
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

        assert_not_has_varargs<f>();
        assert_not_has_varargs<l>();
        assert_not_has_varargs<r>();
        assert_not_has_varargs<c>();
        assert_not_has_varargs<cl>();
        assert_not_has_varargs<cr>();
        assert_not_has_varargs<v>();
        assert_not_has_varargs<vl>();
        assert_not_has_varargs<vr>();
        assert_not_has_varargs<cv>();
        assert_not_has_varargs<cvl>();
        assert_not_has_varargs<cvr>();
    }

    {
        //a member data pointer to a function pointer
        //is not treated like a member function pointer
        using f_ptr = void(*)(...);
        using f   = f_ptr foo::*;
        assert_not_has_varargs<f>();
    }

    {
        using f   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...);
        using l   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) LREF;
        using r   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) RREF ;
        using c   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const;
        using cl  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const LREF;
        using cr  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const RREF;
        using v   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile;
        using vl  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile LREF;
        using vr  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile RREF;
        using cv  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile;
        using cvl = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile LREF;
        using cvr = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile RREF;

        assert_has_varargs<f>();
        assert_has_varargs<l>();
        assert_has_varargs<r>();
        assert_has_varargs<c>();
        assert_has_varargs<cl>();
        assert_has_varargs<cr>();
        assert_has_varargs<v>();
        assert_has_varargs<vl>();
        assert_has_varargs<vr>();
        assert_has_varargs<cv>();
        assert_has_varargs<cvl>();
        assert_has_varargs<cvr>();
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

        assert_not_has_varargs<f>();
        assert_not_has_varargs<l>();
        assert_not_has_varargs<r>();
        assert_not_has_varargs<c>();
        assert_not_has_varargs<cl>();
        assert_not_has_varargs<cr>();
        assert_not_has_varargs<v>();
        assert_not_has_varargs<vl>();
        assert_not_has_varargs<vr>();
        assert_not_has_varargs<cv>();
        assert_not_has_varargs<cvl>();
        assert_not_has_varargs<cvr>();
    }

    {
        struct f   { int operator()(...) { return 0; } };
        struct l   { int operator()(...) LREF { return 0; } };
        struct r   { int operator()(...) RREF { return 0; } };
        struct c   { int operator()(...) const { return 0; } };
        struct cl  { int operator()(...) const LREF { return 0; } };
        struct cr  { int operator()(...) const RREF { return 0; } };
        struct v   { int operator()(...) volatile { return 0; } };
        struct vl  { int operator()(...) volatile LREF { return 0; } };
        struct vr  { int operator()(...) volatile RREF { return 0; } };
        struct cv  { int operator()(...) const volatile { return 0; } };
        struct cvl { int operator()(...) const volatile LREF { return 0; } };
        struct cvr { int operator()(...) const volatile RREF { return 0; } };

        assert_has_varargs<f>();
        assert_has_varargs<l>();
        assert_has_varargs<r>();
        assert_has_varargs<c>();
        assert_has_varargs<cl>();
        assert_has_varargs<cr>();
        assert_has_varargs<v>();
        assert_has_varargs<vl>();
        assert_has_varargs<vr>();
        assert_has_varargs<cv>();
        assert_has_varargs<cvl>();
        assert_has_varargs<cvr>();
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

        CT_ASSERT(! has_varargs<f>());
        CT_ASSERT(! has_varargs<l>());
        CT_ASSERT(! has_varargs<r>());
        CT_ASSERT(! has_varargs<c>());
        CT_ASSERT(! has_varargs<cl>());
        CT_ASSERT(! has_varargs<cr>());
        CT_ASSERT(! has_varargs<v>());
        CT_ASSERT(! has_varargs<vl>());
        CT_ASSERT(! has_varargs<vr>());
        CT_ASSERT(! has_varargs<cv>());
        CT_ASSERT(! has_varargs<cvl>());
        CT_ASSERT(! has_varargs<cvr>());
    }

    {
        using f   = void(...);
        using l   = void(...) LREF;
        using r   = void(...) RREF ;
        using c   = void(...) const;
        using cl  = void(...) const LREF;
        using cr  = void(...) const RREF;
        using v   = void(...) volatile;
        using vl  = void(...) volatile LREF;
        using vr  = void(...) volatile RREF;
        using cv  = void(...) const volatile;
        using cvl = void(...) const volatile LREF;
        using cvr = void(...) const volatile RREF;

        CT_ASSERT( has_varargs<f>());
        CT_ASSERT( has_varargs<l>());
        CT_ASSERT( has_varargs<r>());
        CT_ASSERT( has_varargs<c>());
        CT_ASSERT( has_varargs<cl>());
        CT_ASSERT( has_varargs<cr>());
        CT_ASSERT( has_varargs<v>());
        CT_ASSERT( has_varargs<vl>());
        CT_ASSERT( has_varargs<vr>());
        CT_ASSERT( has_varargs<cv>());
        CT_ASSERT( has_varargs<cvl>());
        CT_ASSERT( has_varargs<cvr>());
    }

#endif //#ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS

    assert_not_has_varargs<void(*)()>();
    assert_has_varargs<void(*)(...)>();
    assert_not_has_varargs<void(&)()>();
    assert_has_varargs<void(&)(...)>();
}
