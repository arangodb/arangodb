/*
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
*/

#include <type_traits>
#include <functional>
#include <tuple>
#include <boost/callable_traits/add_member_lvalue_reference.hpp>
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

        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, f)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, c)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cv)>{});
        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, l)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, cl)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, vl)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cvl)>{});
        CT_ASSERT(!std::is_same<r,    TRAIT(add_member_lvalue_reference, r)>{});
        CT_ASSERT(!std::is_same<cr,   TRAIT(add_member_lvalue_reference, cr)>{});
        CT_ASSERT(!std::is_same<vr,   TRAIT(add_member_lvalue_reference, vr)>{});
        CT_ASSERT(!std::is_same<cvr,  TRAIT(add_member_lvalue_reference, cvr)>{});
    } 
    
    {
        using f   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...);
        using l   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) &;
        using r   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) &&;
        using c   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const;
        using cl  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const &;
        using cr  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const &&;
        using v   = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile;
        using vl  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile &;
        using vr  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) volatile &&;
        using cv  = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile;
        using cvl = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile &;
        using cvr = void(BOOST_CLBL_TRTS_DEFAULT_VARARGS_CC foo::*)(...) const volatile &&;

        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, f)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, c)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cv)>{});
        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, l)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, cl)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, vl)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cvl)>{});
        CT_ASSERT(!std::is_same<r,    TRAIT(add_member_lvalue_reference, r)>{});
        CT_ASSERT(!std::is_same<cr,   TRAIT(add_member_lvalue_reference, cr)>{});
        CT_ASSERT(!std::is_same<vr,   TRAIT(add_member_lvalue_reference, vr)>{});
        CT_ASSERT(!std::is_same<cvr,  TRAIT(add_member_lvalue_reference, cvr)>{});
    }
    
    {
        using f   = void(...);
        using l   = void(...) &;
        using r   = void(...) &&;
        using c   = void(...) const;
        using cl  = void(...) const &;
        using cr  = void(...) const &&;
        using v   = void(...) volatile;
        using vl  = void(...) volatile &;
        using vr  = void(...) volatile &&;
        using cv  = void(...) const volatile;
        using cvl = void(...) const volatile &;
        using cvr = void(...) const volatile &&;

        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, f)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, c)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cv)>{});
        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, l)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, cl)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, vl)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cvl)>{});
        CT_ASSERT(!std::is_same<r,    TRAIT(add_member_lvalue_reference, r)>{});
        CT_ASSERT(!std::is_same<cr,   TRAIT(add_member_lvalue_reference, cr)>{});
        CT_ASSERT(!std::is_same<vr,   TRAIT(add_member_lvalue_reference, vr)>{});
        CT_ASSERT(!std::is_same<cvr,  TRAIT(add_member_lvalue_reference, cvr)>{});
    }

    #ifdef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE
    {
        using f   = void(...) transaction_safe;
        using l   = void(...) & transaction_safe;
        using r   = void(...) && transaction_safe;
        using c   = void(...) const transaction_safe;
        using cl  = void(...) const & transaction_safe;
        using cr  = void(...) const && transaction_safe;
        using v   = void(...) volatile transaction_safe;
        using vl  = void(...) volatile & transaction_safe;
        using vr  = void(...) volatile && transaction_safe;
        using cv  = void(...) const volatile transaction_safe;
        using cvl = void(...) const volatile & transaction_safe;
        using cvr = void(...) const volatile && transaction_safe;

        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, f)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, c)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cv)>{});
        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, l)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, cl)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, vl)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cvl)>{});
        CT_ASSERT(!std::is_same<r,    TRAIT(add_member_lvalue_reference, r)>{});
        CT_ASSERT(!std::is_same<cr,   TRAIT(add_member_lvalue_reference, cr)>{});
        CT_ASSERT(!std::is_same<vr,   TRAIT(add_member_lvalue_reference, vr)>{});
        CT_ASSERT(!std::is_same<cvr,  TRAIT(add_member_lvalue_reference, cvr)>{});

    }
    #endif // #ifdef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE

    #ifdef BOOST_CLBL_TRTS_ENABLE_NOEXCEPT_TYPES
    {
        using f   = void(...) noexcept;
        using l   = void(...) & noexcept;
        using r   = void(...) && noexcept;
        using c   = void(...) const noexcept;
        using cl  = void(...) const & noexcept;
        using cr  = void(...) const && noexcept;
        using v   = void(...) volatile noexcept;
        using vl  = void(...) volatile & noexcept;
        using vr  = void(...) volatile && noexcept;
        using cv  = void(...) const volatile noexcept;
        using cvl = void(...) const volatile & noexcept;
        using cvr = void(...) const volatile && noexcept;

        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, f)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, c)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cv)>{});
        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, l)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, cl)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, vl)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cvl)>{});
        CT_ASSERT(!std::is_same<r,    TRAIT(add_member_lvalue_reference, r)>{});
        CT_ASSERT(!std::is_same<cr,   TRAIT(add_member_lvalue_reference, cr)>{});
        CT_ASSERT(!std::is_same<vr,   TRAIT(add_member_lvalue_reference, vr)>{});
        CT_ASSERT(!std::is_same<cvr,  TRAIT(add_member_lvalue_reference, cvr)>{});
    }
    #endif // #ifdef BOOST_CLBL_TRTS_ENABLE_NOEXCEPT_TYPES

    #ifdef BOOST_CLBL_TRTS_ENABLE_NOEXCEPT_TYPES
    #ifdef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE
    {
        using f   = void(...) transaction_safe noexcept;
        using l   = void(...) & transaction_safe noexcept;
        using r   = void(...) && transaction_safe noexcept;
        using c   = void(...) const transaction_safe noexcept;
        using cl  = void(...) const & transaction_safe noexcept;
        using cr  = void(...) const && transaction_safe noexcept;
        using v   = void(...) volatile transaction_safe noexcept;
        using vl  = void(...) volatile & transaction_safe noexcept;
        using vr  = void(...) volatile && transaction_safe noexcept;
        using cv  = void(...) const volatile transaction_safe noexcept;
        using cvl = void(...) const volatile & transaction_safe noexcept;
        using cvr = void(...) const volatile && transaction_safe noexcept;

        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, f)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, c)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, v)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cv)>{});
        CT_ASSERT(std::is_same<l,     TRAIT(add_member_lvalue_reference, l)>{});
        CT_ASSERT(std::is_same<cl,    TRAIT(add_member_lvalue_reference, cl)>{});
        CT_ASSERT(std::is_same<vl,    TRAIT(add_member_lvalue_reference, vl)>{});
        CT_ASSERT(std::is_same<cvl,   TRAIT(add_member_lvalue_reference, cvl)>{});
        CT_ASSERT(!std::is_same<r,    TRAIT(add_member_lvalue_reference, r)>{});
        CT_ASSERT(!std::is_same<cr,   TRAIT(add_member_lvalue_reference, cr)>{});
        CT_ASSERT(!std::is_same<vr,   TRAIT(add_member_lvalue_reference, vr)>{});
        CT_ASSERT(!std::is_same<cvr,  TRAIT(add_member_lvalue_reference, cvr)>{});
    }
    #endif // #ifdef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE
    #endif // #ifdef BOOST_CLBL_TRTS_ENABLE_NOEXCEPT_TYPES
}

#endif //#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS

